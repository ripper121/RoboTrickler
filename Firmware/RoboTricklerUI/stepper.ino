#define FULL_STEP 1

struct StepperPins
{
  uint8_t dir;
  uint8_t step;
  int rpm;
};

static StepperPins steppers[3] = {
    {0, 0, 0},
    {I2S_X_DIRECTION_PIN, I2S_X_STEP_PIN, 100},
    {I2S_Y_DIRECTION_PIN, I2S_Y_STEP_PIN, 100},
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
    rpm = 100;
  }
  return (unsigned long)(60000000UL / ((unsigned long)MOTOR_STEPS * (unsigned long)FULL_STEP * (unsigned long)rpm));
}

void setStepperSpeed(int stepperNum, int _stepperSpeed)
{
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    return;
  }

  if (_stepperSpeed <= 0)
  {
    _stepperSpeed = 100;
  }
  steppers[stepperNum].rpm = _stepperSpeed;
}

void initStepper()
{
  DEBUG_PRINTLN("initStepper()");
  shiftRegisterInit();
  setStepperSpeed(1, 100);
  setStepperSpeed(2, 100);
  stepperEnableAll(false);
}

void step(int stepperNum, long steps, bool reverse)
{
  if ((stepperNum < 1) || (stepperNum > 2) || (steps == 0))
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
