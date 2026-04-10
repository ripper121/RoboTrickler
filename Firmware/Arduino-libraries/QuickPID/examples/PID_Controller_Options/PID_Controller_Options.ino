/**************************************************************************************
  For testing and checking parameter options. From QuickPID.h:
  Documentation (GitHub): https://github.com/Dlloydev/QuickPID
 **************************************************************************************/

#include <QuickPID.h>

const byte inputPin = 0;
const byte outputPin = 3;

//Define variables we'll be connecting to
float Setpoint = 0, Input = 0, Output = 0, Kp = 2, Ki = 5, Kd = 1;

QuickPID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd,  /* OPTIONS */
               myPID.pMode::pOnError,                   /* pOnError, pOnMeas, pOnErrorMeas */
               myPID.dMode::dOnMeas,                    /* dOnError, dOnMeas */
               myPID.iAwMode::iAwCondition,             /* iAwCondition, iAwClamp, iAwOff */
               myPID.Action::direct);                   /* direct, reverse */

void setup()
{
  Serial.begin(115200);
  delay(2000);

  myPID.SetOutputLimits(0, 255);
  myPID.SetSampleTimeUs(100000);
  myPID.SetTunings(Kp, Ki, Kd);
  myPID.SetMode(myPID.Control::automatic);
  Input = analogRead(inputPin);
  myPID.Compute();
  analogWrite(outputPin, Output);

  Serial.println();
  Serial.print(F(" Setpoint: "));  Serial.println(Setpoint);
  Serial.print(F(" Input:    "));  Serial.println(Input);
  Serial.print(F(" Output:   "));  Serial.println(Output);
  Serial.print(F(" Pterm:    "));  Serial.println(myPID.GetPterm());
  Serial.print(F(" Iterm:    "));  Serial.println(myPID.GetIterm());
  Serial.print(F(" Dterm:    "));  Serial.println(myPID.GetDterm());
  Serial.print(F(" Control:  "));  Serial.println(myPID.GetMode());
  Serial.print(F(" Action:   "));  Serial.println(myPID.GetDirection());
  Serial.print(F(" Pmode:    "));  Serial.println(myPID.GetPmode());
  Serial.print(F(" Dmode:    "));  Serial.println(myPID.GetDmode());
  Serial.print(F(" AwMode:   "));  Serial.println(myPID.GetAwMode());

  analogWrite(outputPin, 0);
}

void loop()
{
}
