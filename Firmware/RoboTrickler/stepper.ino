void setStepperSpeed() {
  stepperX.begin(stepperSpeed, MICROSTEPS);
  stepperX.setEnableActiveState(LOW);
}

void initStepper() {
  stepperSpeed = preferences.getInt("stepperSpeed", 200);
  if (stepperSpeed < 10 || stepperSpeed > 200) {
    stepperSpeed = 10;
    preferences.putInt("stepperSpeed", stepperSpeed);
  }
  setStepperSpeed();
}

void step(int steps) {
  int forward = steps / 2;
  int backward = -(steps / 2);
  stepperX.rotate(forward);
  stepperX.rotate(backward);
}
