// Versja z 29.11.2018 niepełna z dodanymi klasami logowania i debugowania

// for commenting out the basic functions use //temp/ marker... then use ctrl+f to find and delete them

#include <Event.h>
#include <Timer.h>
#include <cStepperMotor_Arduino_retro.h>
#include <cCommandQueue_Arduino.h>
#include <cTimedOccurence_Arduino.h>
#include <BasicSimples.h>
#include <cLogMessage.ino>

class LogMessage {
  // private by default
    int       m_Level = 0;
    const int LevelNone = -1;
    const int LevelLog = 0;
    const int LevelWarn = 1;
    const int LevelError = 2;
 public:
    void      MessageLog(String msg) { if (m_Level >= 0) { Serial.println(msg); } }
    void      MessageWarn(String msg) { if (m_Level >= 1) { Serial.println(msg); } }
    void      MessageError(String msg) { if (m_Level = 2) { Serial.println(msg); } }
    void      SetLevel(int level) { m_Level = level; }
}Log;

class debugTimer {
 // private by default
 unsigned long          m_startTime;
 unsigned long          m_endTime;
 public:
        debugTimer()    { m_startTime = millis(); }
        ~debugTimer()   { m_endTime = millis(); m_endTime = m_endTime - m_startTime; Serial.print("Time elapsed :"); Serial.println(m_endTime); }
}dTimer;



class Tonometer {
  private:
    cStepperMotor&        m_cMotor;     // stepper motor object
    SimpleDigitalDevice&  m_cMotorEn;   // stepper motor object enabling pin
    SimpleSensor&         m_cSensor;    // pressure sensor object
    SimpleSwitch&         m_cswStart;   // start switch
    SimpleSwitch&         m_cswReset;   // axis switch

    int             m_startValue;
    long int        m_scanLength;
    int             m_scanArea;
    double          m_scanTime;

    int temp_for_Me = 0; 
    
    enum F_COND : int
    {
      F_INITON  = 1 << 1,
      F_SENDING = 1 << 2,
      F_SENDDAT = 1 << 3,
      F_FAILSCN = 1 << 4,
    };
    
    enum E_MODE : int
    {
      E_NONE,               // brak stanu, czekanie
      E_START,              // rozpoczÄ™cie skanu
      E_REAXIS,             // reset osi do punktu zerowego
      E_VALCHECK,           // sprawdzenie wartoĹ›ci i punktu ruchu czy nie przekracza maksimum
      E_FAILSCAN,           // skan z jakiegoĹ› powodu nieudany, reset wartoĹ›ci i rekalibracja
      E_MOVEMENT,           // ruch do okreĹ›lonego punktu
      E_STATIONARY,         // brak ruchu, skan w toku
      E_RETURN,
      E_FINISH,             // koniec skanu, wysĹ‚anie znacznika i powrĂłt do 0
    };
    
    int       m_Flags;
    E_MODE    m_State;
    
  public:
                  Tonometer(cStepperMotor& Motor, SimpleDigitalDevice& MotorEn, SimpleSensor& Sensor, SimpleSwitch& swStart, SimpleSwitch& swReset, long int scanLength, int scanArea, int startValue)
                              :
                              m_cMotor  (Motor),
                              m_cMotorEn(MotorEn),    // do wyrzucenia gdy w klasie silnikĂłw krokowych dodam obsĹ‚ugÄ™ wĹ‚Ä…czania prÄ…du w sterowniku
                              m_cSensor   (Sensor),
                              m_cswStart  (swStart),
                              m_cswReset  (swReset),
                              m_scanLength  (scanLength),
                              m_scanArea    (scanArea),
                              m_startValue  (startValue)
                              { m_scanTime = 0; };
                              
  bool      CheckDataFlow() { if (m_Flags & F_SENDING) { return true; } else { return false; } }  // checks if Tonometer is sending unregistered data
  bool      CheckDataSend() { if (m_Flags & F_SENDDAT) { return true; } else { return false; } }  // checks if Tonometer is making a registered scan

  void      ChangeRecordLength  (long int scanLength) { m_scanLength = scanLength; } // changes length of scan in miliseconds, meaning overall time ()
  void      ChangeRecordValue   (int startValue)  { m_startValue = startValue; } // changes the value of sensor at which we interpret it as "patient found, send data"
  void      ChangeRecordArea    (int scanArea)   { m_scanArea   = scanArea;   } // changes the area over which the scan goes so until the pressure hits + m_scanArea
  void      ChangeMovementVal   (int moveTo)    { m_cMotor.asmGoToMax(); }

  unsigned long       GetRecordLength() { return m_scanLength; }
  int                 GetRecordValue()  { return m_startValue; }
  int                 GetRecordArea()   { return m_scanArea;   }
    
  void      StartScan()       { if (m_State == E_MODE::E_NONE) { m_State = E_MODE::E_START; } }

  void      CheckStartSwitch(){ if (m_cswStart.GetState()) { this->StartScan(); } }
  
  void      OpenConn  ()      { m_State = E_MODE::E_NONE; m_Flags =  F_COND::F_INITON |  F_COND::F_SENDING | ~F_COND::F_SENDDAT | ~F_COND::F_FAILSCN; }
  void      CloseConn ()      { m_State = E_MODE::E_NONE; m_Flags = ~F_COND::F_INITON | ~F_COND::F_SENDING | ~F_COND::F_SENDDAT | ~F_COND::F_FAILSCN; }  //
  void      Restart   ()      { this->CloseConn(); this->OpenConn(); }
    
  void      AxisRestart()
  {
    m_cMotorEn.On();
    if (!m_cswReset.GetState()) // jeĹ›li nie wciĹ›niÄ™ty switch
    {
      m_cMotor.asmGoToMin();
      while (!m_cswReset.GetState()) { } // czeka aĹĽ zejdzie ze switcha
    }

    m_cMotor.asmGoToMax();

    while (m_cswReset.GetState()) { }

    m_cMotor.asmStopMovement();
    m_cMotor.asmMoveTo(m_cMotor.asmGetStep()+400);
    m_cMotor.asmSetZero(); m_cMotor.asmSetMin(-1000);
    
    //while (m_cMotor.asmGetState() != cStepperMotor::E_ErrorCode::DestReached) { }
    
    m_cMotorEn.Off();
  }

    void          RunData()
    {
                if (m_Flags & F_COND::F_INITON & ~F_COND::F_FAILSCN)   // dodaÄ‡ streamowanie w formie stringa jednego ktory jest wysylany po zlozeniu ifow
                {
                        String data = "";
                        if (m_Flags & F_COND::F_SENDING)        // dane streamowane bez zapisu
                        {
                                Serial.print(m_cSensor.GetIntValue(), DEC);
                                data = data+"DT";
                                Serial.print(data);
                        }
                        //if (m_Flags & F_COND::F_SENDDAT)        // marker do zapisu
                        //{
                        //        data = data+"S";
                        //}
                        // if (m_Flags & (F_COND::F_SENDDAT & F_COND::F_SENDING)) { }
                }
     }
  
     void       TonometerRun()
     {

     this->CheckStartSwitch();
        
     if (m_Flags & F_COND::F_INITON & ~F_COND::F_FAILSCN)
           {
            switch(m_State)
            {
                case E_MODE::E_START:
                  {
                    m_State = E_MODE::E_REAXIS;
                    break;
                  }
                case E_MODE::E_REAXIS:
                  {
                    this->AxisRestart();
                    Serial.print("_BG_");                       // begin
                    m_cMotorEn.On();
                    m_cMotor.asmGoToMax();
                    m_State = E_MODE::E_VALCHECK;
                    break;
                  }
                case E_MODE::E_VALCHECK:
                  {
                    // fail jak za daleko pojechal
                    if (m_cMotor.asmGetStep()+m_scanArea > m_cMotor.asmGetMax()) { m_State = E_MODE::E_FAILSCAN; }

                    // porownaj wartosc sensora z progiem
                    if (m_cSensor.GetIntValue() >= m_startValue)
                      {
                        temp_for_Me = m_cMotor.asmGetStep();
                        m_cMotor.asmMoveTo(temp_for_Me+m_scanArea);

                        // value reached
                        Serial.print("_VT_");          // marker 1
                        m_Flags |= F_COND::F_SENDDAT;
                        m_State = E_MODE::E_MOVEMENT;
                      }
                    break;
                  }
                case E_MODE::E_FAILSCAN:
                {
                    Serial.print("_FL_");
                    m_cMotor.asmStopMovement();
                    m_Flags |= F_COND::F_FAILSCN;
                    m_State = E_MODE::E_NONE;
                    m_cMotorEn.Off(); // tu jest jakiś problem, się nie wykonuje jak failnie
                    break;
                }
                case E_MODE::E_MOVEMENT:
                  {
                    if ( m_cMotor.asmGetState() == 0 )
                    {
                      Serial.print("_PR_");   //  movement  // marker 2
                      m_scanTime = millis();
                      m_State = E_MODE::E_STATIONARY;
                    }
                    break;
                  }
                case E_MODE::E_STATIONARY:
                  {
                    if ((m_scanTime+(double)m_scanLength) < millis())
                    {
                      Serial.print("_SE_");   //  stationary  // marker 3
                      m_State = E_MODE::E_RETURN;
                      m_cMotor.asmGoToZero();
                    }
                    break;
                  }
                case E_MODE::E_RETURN:
                  {
                      if ( m_cMotor.asmGetStep() <= temp_for_Me)
                      {
                      Serial.print("_SR_");   //  stationary // marker 4
                      m_State = E_MODE::E_FINISH;
                      }
                      break;
                  }
                case E_MODE::E_FINISH:
                  {
                    if ( m_cMotor.asmGetState() == 0 )
                    {
                      Serial.print("_RR_");   // scan end
                      m_Flags = F_COND::F_INITON | F_COND::F_SENDING | ~F_COND::F_SENDDAT | ~F_COND::F_FAILSCN;
                      m_State = E_MODE::E_NONE;
                      m_cMotorEn.Off();
                    }
                    break; 
                  }
                                default: { break; }
                        }
                }
        
     }
};

// REAL PROGRAM CODE -------------------------------------------------------------

#define pinMotEn    8
#define pinMs1      9
#define pinMs2      10
#define pinStep     11
#define pinDir      12

#define pinSwitch   4 // switch to pin 7, przewod zolty
#define pinStart    5  // start to pin 6, przewod pomaranczowy
#define pinData     A0

cTimedOccurence     myTimer;
cStepperMotor       myMotor(pinStep, pinDir, -25000, 13000);

SimpleSwitch        swStart(pinStart, true);
SimpleSwitch        swMReset(pinSwitch, true);
SimpleSensor        seData(pinData, 4.78);  // pin i wartoĹ›Ä‡ w volt do uzyskania wynikĂłw, zwykle podaje siÄ™ 5V
SimpleDigitalDevice myMotorEn(pinMotEn, true); // to moĹĽna wrzuciÄ‡ do klasy silnika krokowego... przerobiÄ‡ w wolnym czasie

// timery do 
Timer   Tmr;
int     TimerEvent;

String        dev_SerialNumber = "XY03";
int           motorFrequency  = 400;
long int      scanLength      = 10000L; // 10sec

void SendDataFunc();

Tonometer           myTono(myMotor, myMotorEn, seData, swStart, swMReset, scanLength, 1, 150);

class TonometerMessageQueue : public cCommandQueue
{
    private:
      void  StartTonometer()
            {
              if (this->cmd_CheckExist("A_TN?"))
              {
                Serial.print("A_TN!");
                TimerEvent = Tmr.every(10, SendDataFunc);
                //myMotorEn.On();
                myTono.OpenConn();
                //temp/myTono.AxisRestart();
              }
            }
///////////////////////////////////////////////
       void  MotorOn()
            {
              if (this->cmd_CheckExist("A_MT+"))
              {
                myMotorEn.On();
              }
            }

       void  MotorOff()
            {
              if (this->cmd_CheckExist("A_MT-"))
              {
                myMotorEn.Off();
              }
            }
      void MoveByValue()
            {
              long int innerValue = 0;
              if (this->cmd_CheckForInnerValue("M_","_VM", innerValue))
              {
                myTono.ChangeMovementVal(innerValue);
                Serial.println("move");
              }
            }
//////////////////////////////////////////////////
      void  Check_ChangeThreshold()
            {
              long int innerValue = 0;
              if (this->cmd_CheckForInnerValue("T_","_VS", innerValue))
              {
                if (innerValue >= 0)
                {
                  myTono.ChangeRecordValue(innerValue);
                  Serial.print("T_"+String(myTono.GetRecordValue())+"_VR");
                }
                else
                {
                  Serial.print("T_"+String(myTono.GetRecordValue())+"_VR");
                }
              }
            }
         void  Check_ChangeArea()
            {
              long int innerValue = 0;
              if (this->cmd_CheckForInnerValue("A_","_VS", innerValue))
              {
                if (innerValue >= 0)
                {
                  myTono.ChangeRecordArea((int)innerValue);
                  Serial.print("A_"+String(myTono.GetRecordArea())+"_VR");
                }
                else
                {
                  Serial.print("A_"+String(myTono.GetRecordArea())+"_VR");
                }
              }
            }
         void  Check_ChangeTime()
            {
              long int innerValue = 0;
              if (this->cmd_CheckForInnerValue("L_","_VS", innerValue))
              {
                if (innerValue >= 0)
                {
                  myTono.ChangeRecordLength(innerValue);
                  Serial.print("L_"+String(myTono.GetRecordLength())+"_VR");
                }
                else
                {
                  Serial.print("L_"+String(myTono.GetRecordLength())+"_VR");
                }
              }
            }
         void  Check_StepsPerRev()
         {
            if (this->cmd_CheckExist("S_REV"))
            {
              Serial.print("S_"+String(motorFrequency)+"_VR");
            }
         }
         void  Check_SerialNumber()
         {
            if (this->cmd_CheckExist("Q_S?"))
            {
              Serial.print("S!_"+dev_SerialNumber+"_SN");
            }
         }
      void  StopTonometer()
            {
              if (this->cmd_CheckExist("A_Q!"))
              {
                Tmr.stop(TimerEvent);
                myTono.AxisRestart();
                myMotorEn.Off();
                myTono.CloseConn();
                Serial.print("A_QT!");
              }
            }
    public:
      void  cmd_Parse()
            {
              this->StartTonometer();
              this->Check_ChangeThreshold();
              this->Check_ChangeArea();
              this->Check_ChangeTime();
              this->Check_StepsPerRev();
              this->Check_SerialNumber();
              this->StopTonometer();
              
              // test/debug functions
              this->MotorOn();
              this->MotorOff();
              this->MoveByValue();
            }
}Queue;


void setup()
{
  Queue.Initialize(57600);
  myMotorEn.Off();
  myMotor.asmSetDir(false);  // zmienione dla nowego, dla starego true
  while (!Serial) {} //wait for serial
  
  myTimer.Setup_16bit(motorFrequency);
}

void loop()
{
  Tmr.update();
  Queue.cqRun();
  myTono.TonometerRun();
}

void SendDataFunc()
{
  //temp/myTono.RunData();
}

ISR(TIMER1_COMPA_vect)
{
  //temp/myMotor.asmRun();
}
