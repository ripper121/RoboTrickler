/********************************************************
   PID AVR Basic Software TIMER Example
   Reading analog input 0 to control analog PWM output 3
 ********************************************************/
#include <Ticker.h> // https://github.com/sstaub/Ticker
#include <QuickPID.h>
void runPid();

#define PIN_INPUT 0
#define PIN_OUTPUT 3
const uint32_t sampleTimeUs = 100000; // 100ms
static boolean computeNow = false;

//Define Variables we'll be connecting to
float Setpoint, Input, Output;

//Specify the links and initial tuning parameters
float Kp = 2, Ki = 5, Kd = 1;

Ticker timer1(runPid, sampleTimeUs, 0, MICROS_MICROS);
QuickPID myPID(&Input, &Output, &Setpoint);

void setup() {
  timer1.start();

  //initialize the variables we're linked to
  Input = analogRead(PIN_INPUT);
  Setpoint = 100;

  //apply PID gains
  myPID.SetTunings(Kp, Ki, Kd);

  //turn the PID on
  myPID.SetMode(myPID.Control::automatic);
}

void loop() {
  timer1.update();
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
