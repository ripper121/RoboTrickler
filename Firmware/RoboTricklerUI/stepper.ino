void setStepperSpeed(int stepperNum, int _stepperSpeed)
{
  if (stepperNum == 1)
  {
    stepper1.begin(_stepperSpeed, config.microsteps);
    stepper1.setEnableActiveState(LOW);
  }
  if (stepperNum == 2)
  {
    stepper2.begin(_stepperSpeed, config.microsteps);
    stepper2.setEnableActiveState(LOW);
  }
}

void initStepper()
{
  DEBUG_PRINTLN("initStepper()");
  setStepperSpeed(1, 100);
  setStepperSpeed(2, 100);
}

void step(int stepperNum, int steps, bool oscillate, bool reverse, bool acceleration)
{
  if (stepperNum == 1)
    stepper1.enable();
  if (stepperNum == 2)
    stepper2.enable();

  if (stepperNum == 1)
  {
    if (acceleration)
      stepper1.setSpeedProfile(stepper1.LINEAR_SPEED, 1000, 1000);
    else
      stepper1.setSpeedProfile(stepper1.CONSTANT_SPEED, 1000, 1000);
  }

  if (stepperNum == 2)
  {
    if (acceleration)
      stepper2.setSpeedProfile(stepper2.LINEAR_SPEED, 1000, 1000);
    else
      stepper2.setSpeedProfile(stepper2.CONSTANT_SPEED, 1000, 1000);
  }

  steps = steps * config.microsteps;
  if (steps > 10 && (oscillate))
  {
    int forward = steps / 2;
    int backward = -(steps / 2);
    if (stepperNum == 1)
    {
      stepper1.rotate(forward);
      stepper1.rotate(backward);
    }
    if (stepperNum == 2)
    {
      stepper2.rotate(forward);
      stepper2.rotate(backward);
    }
  }
  else
  {
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
  }

  stepper1.disable();
  stepper2.disable();
}
