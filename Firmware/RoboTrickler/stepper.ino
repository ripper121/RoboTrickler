void setStepperSpeed(int _stepperSpeed) {
  stepperX.begin(_stepperSpeed, MICROSTEPS);
  stepperX.setEnableActiveState(LOW);
}

void initStepper() {
  Serial.println("initStepper()");
  setStepperSpeed(100);
}

void step(int steps) {
  if (steps > 10) {
    int forward = steps / 2;
    int backward = -(steps / 2);
    stepperX.rotate(forward);
    stepperX.rotate(backward);
  } else{
    stepperX.rotate(steps);
  }
}
