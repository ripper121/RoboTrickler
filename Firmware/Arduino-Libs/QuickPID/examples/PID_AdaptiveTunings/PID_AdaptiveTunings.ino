/******************************************************************************
   PID Adaptive Tuning Example
   One of the benefits of the PID library is that you can change the tuning
   parameters at any time. This can be helpful if we want the controller to
   be agressive at some times and conservative at others. In the example below,
   we set the controller to use conservative tuning parameters when we're near
   setpoint and more agressive tuning parameters when we're farther away.
 ******************************************************************************/

#include <QuickPID.h>

#define PIN_INPUT 0
#define PIN_OUTPUT 3

//Define Variables we'll be connecting to
float Setpoint, Input, Output;

//Define the aggressive and conservative and POn Tuning Parameters
float aggKp = 4, aggKi = 0.2, aggKd = 1;
float consKp = 1, consKi = 0.05, consKd = 0.25;

//Specify the links
QuickPID myPID(&Input, &Output, &Setpoint);

void setup()
{
  //initialize the variables we're linked to
  Input = analogRead(PIN_INPUT);
  Setpoint = 100;

  //turn the PID on
  myPID.SetMode(myPID.Control::automatic);
}

void loop()
{
  Input = analogRead(PIN_INPUT);

  float gap = abs(Setpoint - Input); //distance away from setpoint
  if (gap < 10) { //we're close to setpoint, use conservative tuning parameters
    myPID.SetTunings(consKp, consKi, consKd);
  } else {
    //we're far from setpoint, use aggressive tuning parameters
    myPID.SetTunings(aggKp, aggKi, aggKd);
  }
  myPID.Compute();
  analogWrite(PIN_OUTPUT, Output);
}
