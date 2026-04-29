#define FULL_STEP 1

void setStepperSpeed(int stepperNum, int _stepperSpeed)
{
  if (stepperNum == 1)
  {
    stepper1.begin(_stepperSpeed, FULL_STEP);
    stepper1.setEnableActiveState(LOW);
  }
  if (stepperNum == 2)
  {
    stepper2.begin(_stepperSpeed, FULL_STEP);
    stepper2.setEnableActiveState(LOW);
  }
}

void initStepper()
{
  DEBUG_PRINTLN("initStepper()");
  setStepperSpeed(1, 100);
  setStepperSpeed(2, 100);
  stepper1.disable();
  stepper2.disable();
}

void step(int stepperNum, int steps, bool reverse)
{
  if (stepperNum == 1)
    stepper1.enable();
  if (stepperNum == 2)
    stepper2.enable();

  if (stepperNum == 1)
  {
    stepper1.setSpeedProfile(stepper1.CONSTANT_SPEED, ACCEL, 32767);
  }

  if (stepperNum == 2)
  {
    stepper2.setSpeedProfile(stepper2.CONSTANT_SPEED, ACCEL, 32767);
  }

  if (reverse)
  {
    steps = steps * (-1);
  }
  if (stepperNum == 1)
  {
    stepper1.rotate(steps);
  }
  if (stepperNum == 2)
  {
    stepper2.rotate(steps);
  }

  stepper1.disable();
  stepper2.disable();
}
