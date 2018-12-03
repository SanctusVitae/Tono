
#ifndef _TONO_DYNAMICS_H_
#define _TONO_DYNAMICS_H_

#include <Arduino.h>
#include <cStepperMotor_Arduino_retro.h>
#include <cCommandQueue_Arduino.h>
#include <cTimedOccurence_Arduino.h>
#include <BasicSimples.h>

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
    if (!m_cswReset.GetState()) // jeśli nie jest na switchu
    {
      m_cMotor.asmGoToMin();
      while (!m_cswReset.GetState()) { } // czeka do najechania na switch
    }

    m_cMotor.asmGoToMax();

    

    while (m_cswReset.GetState()) { }
    
    m_cMotor.asmStopMovement();
    m_cMotor.asmMoveTo(m_cMotor.asmGetStep()+400);
    delay(1);
    while (!m_cMotor.asmCheckDestReached()) { delay(1); }
    
    m_cMotor.asmSetZero(); m_cMotor.asmSetMin(-1000);
    
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
                        m_cMotor.asmMoveTo(m_cMotor.asmGetStep()+m_scanArea);

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
                    if (m_cMotor.asmCheckDestReached())
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
                      if (m_cMotor.asmCheckDestReached())
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

#endif
