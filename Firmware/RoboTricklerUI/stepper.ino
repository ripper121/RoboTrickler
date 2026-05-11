#define FULL_STEP 1

struct StepperPins
{
  uint8_t dir;
  uint8_t step;
  int rpm;
};

static StepperPins steppers[PROFILE_ACTUATOR_SLOTS] = {
    {0, 0, 0},
    {I2S_X_DIRECTION_PIN, I2S_X_STEP_PIN, DEFAULT_STEPPER_RPM},
    {I2S_Y_DIRECTION_PIN, I2S_Y_STEP_PIN, DEFAULT_STEPPER_RPM},
};

static uint32_t shiftRegisterState = 0;
static bool shiftRegisterReady = false;
static portMUX_TYPE shiftRegisterMux = portMUX_INITIALIZER_UNLOCKED;

static void shiftRegisterFlush()
{
  digitalWrite(I2S_OUT_WS, LOW);
  for (int i = 0; i < I2S_OUT_NUM_BITS; i++)
  {
    uint32_t mask = 1UL << (I2S_OUT_NUM_BITS - 1 - i);
    digitalWrite(I2S_OUT_DATA, (shiftRegisterState & mask) ? HIGH : LOW);
    digitalWrite(I2S_OUT_BCK, HIGH);
    digitalWrite(I2S_OUT_BCK, LOW);
  }
  digitalWrite(I2S_OUT_WS, HIGH);
}

static void shiftRegisterWrite(uint8_t pin, bool level)
{
  if (pin >= I2S_OUT_NUM_BITS)
  {
    return;
  }

  portENTER_CRITICAL(&shiftRegisterMux);
  if (level)
  {
    shiftRegisterState |= (1UL << pin);
  }
  else
  {
    shiftRegisterState &= ~(1UL << pin);
  }
  shiftRegisterFlush();
  portEXIT_CRITICAL(&shiftRegisterMux);
}

static void shiftRegisterInit()
{
  if (shiftRegisterReady)
  {
    return;
  }

  pinMode(I2S_OUT_BCK, OUTPUT);
  pinMode(I2S_OUT_WS, OUTPUT);
  pinMode(I2S_OUT_DATA, OUTPUT);
  digitalWrite(I2S_OUT_BCK, LOW);
  digitalWrite(I2S_OUT_WS, LOW);
  digitalWrite(I2S_OUT_DATA, LOW);

  shiftRegisterState = 0;
  shiftRegisterState |= (1UL << I2S_X_DISABLE_PIN);
  shiftRegisterState |= (1UL << I2S_X_DIRECTION_PIN);
  shiftRegisterState |= (1UL << I2S_Y_DIRECTION_PIN);
  shiftRegisterFlush();
  shiftRegisterReady = true;
}

static void stepperEnableAll(bool enable)
{
  shiftRegisterWrite(I2S_X_DISABLE_PIN, enable ? LOW : HIGH);
}

static unsigned long stepperPulseIntervalUs(int rpm)
{
  if (rpm <= 0)
  {
    rpm = DEFAULT_STEPPER_RPM;
  }
  return (unsigned long)(60000000UL / ((unsigned long)MOTOR_STEPS * (unsigned long)FULL_STEP * (unsigned long)rpm));
}

static long calculateStepperStepsForUnits(double remainingUnits, double unitsPerThrow, double *outUnits)
{
  if (outUnits != NULL)
  {
    *outUnits = 0.0;
  }
  if ((remainingUnits <= 0.0) || (unitsPerThrow <= 0.0))
  {
    return 0;
  }

  double exactSteps = remainingUnits * ((double)MOTOR_STEPS / unitsPerThrow);
  if ((exactSteps <= 0.0) || (exactSteps > 2147483647.0))
  {
    return 0;
  }

  long steps = (long)exactSteps;
  if (outUnits != NULL)
  {
    *outUnits = ((double)steps * unitsPerThrow) / (double)MOTOR_STEPS;
  }
  return steps;
}

bool runBulkStepperMove(String &infoText)
{
  double remainingUnits = (double)config.targetWeight - (double)weight - (double)config.profile_weightGap;
  if (remainingUnits <= 0.0)
  {
    return true;
  }

  for (int stepperNum = PROFILE_ACTUATOR_MAX; stepperNum >= PROFILE_ACTUATOR_MIN; stepperNum--)
  {
    ProfileActuator &actuator = config.profile_actuator[stepperNum];
    if (!actuator.enabled)
    {
      continue;
    }

    int speed = actuator.unitsPerThrowSpeed;
    if (speed <= 0)
    {
      speed = DEFAULT_PROFILE_SPEED;
    }

    double units = 0.0;
    long stepsToMove = calculateStepperStepsForUnits(remainingUnits, actuator.unitsPerThrow, &units);
    if (stepsToMove <= 0)
    {
      continue;
    }

    setStepperSpeed(stepperNum, speed);
    step(stepperNum, stepsToMove, false);
    remainingUnits -= units;
    if (remainingUnits < 0.0)
    {
      remainingUnits = 0.0;
    }

    infoText += "B";
    infoText += String(stepperNum);
    infoText += " ST";
    infoText += String(stepsToMove);
    infoText += " ";
  }

  return true;
}

void setStepperSpeed(int stepperNum, int stepperSpeed)
{
  if ((stepperNum < PROFILE_ACTUATOR_MIN) || (stepperNum > PROFILE_ACTUATOR_MAX))
  {
    return;
  }

  if (stepperSpeed <= 0)
  {
    stepperSpeed = DEFAULT_STEPPER_RPM;
  }
  steppers[stepperNum].rpm = stepperSpeed;
}

void initStepper()
{
  DEBUG_PRINTLN("initStepper()");
  shiftRegisterInit();
  for (int stepperNum = PROFILE_ACTUATOR_MIN; stepperNum <= PROFILE_ACTUATOR_MAX; stepperNum++)
  {
    setStepperSpeed(stepperNum, DEFAULT_STEPPER_RPM);
  }
  stepperEnableAll(false);
}

void step(int stepperNum, long steps, bool reverse)
{
  if ((stepperNum < PROFILE_ACTUATOR_MIN) || (stepperNum > PROFILE_ACTUATOR_MAX) || (steps == 0))
  {
    return;
  }

  StepperPins *motor = &steppers[stepperNum];

  if (reverse)
  {
    steps = steps * (-1);
  }

  bool forward = steps >= 0;
  long pulseCount = (labs(steps) * (long)MOTOR_STEPS * (long)FULL_STEP) / 360L;
  if (pulseCount <= 0)
  {
    return;
  }

  unsigned long intervalUs = stepperPulseIntervalUs(motor->rpm);
  if (intervalUs < 2)
  {
    intervalUs = 2;
  }

  stepperEnableAll(true);
  shiftRegisterWrite(motor->dir, forward ? HIGH : LOW);
  delayMicroseconds(2);

  for (long i = 0; i < pulseCount; i++)
  {
    unsigned long start = micros();
    shiftRegisterWrite(motor->step, HIGH);
    delayMicroseconds(2);
    shiftRegisterWrite(motor->step, LOW);

    while ((micros() - start) < intervalUs)
    {
      if ((intervalUs - (micros() - start)) > 50)
      {
        yield();
      }
    }
  }

  stepperEnableAll(false);
}

void stepperBeep(unsigned long timeMs)
{
  shiftRegisterInit();
  shiftRegisterWrite(I2S_BEEPER, HIGH);
  delay(timeMs);
  shiftRegisterWrite(I2S_BEEPER, LOW);
}
