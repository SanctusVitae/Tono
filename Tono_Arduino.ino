// Versja z 29.11.2018 niepełna z dodanymi klasami logowania i debugowania

// for commenting out the basic functions use //temp/ marker... then use ctrl+f to find and delete them

#include <Event.h>
#include <Timer.h>
#include "log_message.cpp"
#include "tono_dynamics.cpp"


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

LogMessage Log;

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
