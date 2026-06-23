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
  // The GRBL-style board drives motor, enable, and beeper lines through a
  // 32-bit serial shift register rather than direct GPIO pins.
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
  return (unsigned long)(60000000UL / ((unsigned long)config.motorStepsPerRev * (unsigned long)rpm));
}

void setStepperRpm(int stepperNum, int stepperRpm)
{
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    return;
  }

  if (stepperRpm <= 0)
  {
    stepperRpm = 100;
  }
  steppers[stepperNum].rpm = stepperRpm;
}

void initStepper()
{
  DEBUG_PRINTLN("initStepper()");
  shiftRegisterInit();
  setStepperRpm(1, 100);
  setStepperRpm(2, 100);
  stepperEnableAll(false);
}

void step(int stepperNum, long steps)
{
  // A positive move always uses the configured direction pin level. Profiles
  // encode direction through actuator choice, not signed step counts.
  if ((stepperNum < 1) || (stepperNum > 2) || (steps <= 0))
  {
    return;
  }

  StepperPins *motor = &steppers[stepperNum];
  long pulseCount = steps;

  unsigned long intervalUs = stepperPulseIntervalUs(motor->rpm);
  if (intervalUs < 2)
  {
    intervalUs = 2;
  }

  stepperEnableAll(true);
  shiftRegisterWrite(motor->dir, HIGH);
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
