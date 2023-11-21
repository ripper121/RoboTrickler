/****************************************************************************
  PID Relay Output Example
  https://github.com/Dlloydev/QuickPID/tree/master/examples/PID_RelayOutput

  Similar to basic example, except the output is a digital pin controlling
  a mechanical relay, SSR, MOSFET or other device. To interface the PID output
  to a digital pin, we use "time proportioning control" (software PWM).
  First we decide on a window size (5000mS for example). We then set the pid
  to adjust its output between 0 and that window size and finally we set the
  PID sample time to that same window size.

  The digital output has the following features:
  • The PID compute rate controls the rate of updating the digital output
  • All transitions are debounced (rising and falling)
  • Full control range (0 to windowSize) isn't limited by debounce
  • Only one call to digitalWrite() per transition
  *****************************************************************************/

#include <QuickPID.h>

// pins
const byte inputPin = 0;
const byte relayPin = 3;

// user settings
const unsigned long windowSize = 5000;
const byte debounce = 50;
float Input, Output, Setpoint = 30, Kp = 2, Ki = 5, Kd = 1;

// status
unsigned long windowStartTime, nextSwitchTime;
boolean relayStatus = false;

QuickPID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd,
               myPID.pMode::pOnError,
               myPID.dMode::dOnMeas,
               myPID.iAwMode::iAwClamp,
               myPID.Action::direct);

void setup() {
  pinMode(relayPin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  myPID.SetOutputLimits(0, windowSize);
  myPID.SetSampleTimeUs(windowSize * 1000);
  myPID.SetMode(myPID.Control::automatic);
}

void loop() {
  unsigned long msNow = millis();
  Input = analogRead(inputPin);
  if (myPID.Compute()) windowStartTime = msNow;

  if (!relayStatus && Output > (msNow - windowStartTime)) {
    if (msNow > nextSwitchTime) {
      nextSwitchTime = msNow + debounce;
      relayStatus = true;
      digitalWrite(relayPin, HIGH);
      digitalWrite(LED_BUILTIN, HIGH);
    }
  } else if (relayStatus && Output < (msNow - windowStartTime)) {
    if (msNow > nextSwitchTime) {
      nextSwitchTime = msNow + debounce;
      relayStatus = false;
      digitalWrite(relayPin, LOW);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}
