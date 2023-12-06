/********************************************************
   PID AVR Basic Interrupt TIMER Example
   Reading analog input 0 to control analog PWM output 3
 ********************************************************/
#include <TimerOne.h> // https://github.com/PaulStoffregen/TimerOne
#include <QuickPID.h>

#define PIN_INPUT 0
#define PIN_OUTPUT 3
const uint32_t sampleTimeUs = 100000; // 100ms
volatile bool computeNow = false;

//Define Variables we'll be connecting to
float Setpoint, Input, Output;

float Kp = 2, Ki = 5, Kd = 1;

//specify the links
QuickPID myPID(&Input, &Output, &Setpoint);

void setup() {
  Timer1.initialize(sampleTimeUs); //initialize timer1, and set the time interval
  Timer1.attachInterrupt(runPid);  //attaches runPid() as a timer overflow interrupt

  //initialize the variables we're linked to
  Input = analogRead(PIN_INPUT);
  Setpoint = 100;

  //apply PID gains
  myPID.SetTunings(Kp, Ki, Kd);

  //turn the PID on
  myPID.SetMode(myPID.Control::automatic);
}

void loop() {
  if (computeNow) {
    Input = analogRead(PIN_INPUT);
    myPID.Compute();
    analogWrite(PIN_OUTPUT, Output);
    computeNow = false;
  }
}

void runPid() {
  computeNow = true;
}
