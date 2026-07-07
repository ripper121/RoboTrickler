// ---------------------------------------------------------------------------
// I2S shift-register stepper driver
//
// The GRBL-style board clocks its 32-bit output shift register with BCK/WS/
// DATA — exactly the I2S TX signal set, which is what these boards are designed
// for. The I2S peripheral streams 32-bit words MSB-first; the register shifts
// on rising BCK and latches on the rising WS edge after a complete word. In MSB
// (left-justified) stereo mode WS rises exactly on the word boundary, so every
// latch captures one complete word. The same state word is written to both
// stereo slots, so it does not matter which slot a latch lands on.
//
// One frame = one latched output update = I2S_OUT_FRAME_US microseconds, DMA
// paced with zero CPU jitter. Step pulses are generated as samples: one frame
// with the step bit high, then interval/I2S_OUT_FRAME_US - 1 frames low, so the
// step rate is quantized to the frame clock (0.5 frame worst-case rounding —
// well under 1% at trickler speeds). A low-priority feeder task streams the
// current state word between moves so enable/direction/beeper writes behave
// like direct pin writes, just latched a few milliseconds (the DMA buffer
// depth) later; relative ordering of all writes is preserved because everything
// flows through the same sample stream.
// ---------------------------------------------------------------------------

#include <driver/i2s_std.h>

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

// Initial register state: motors disabled (bit is active-high), both direction
// lines at their configured positive level.
#define SHIFT_REGISTER_DEFAULT_STATE ((1UL << I2S_X_DISABLE_PIN) | \
                                      (1UL << I2S_X_DIRECTION_PIN) | \
                                      (1UL << I2S_Y_DIRECTION_PIN))

#define I2S_OUT_SAMPLE_RATE_HZ 125000 // frames per second; one latch per frame
#define I2S_OUT_FRAME_US (1000000UL / I2S_OUT_SAMPLE_RATE_HZ)
#define I2S_OUT_BLOCK_FRAMES 256      // ~2 ms of stream per write call
#define I2S_OUT_DMA_DESC_NUM 4        // 4 x 256 frames = ~8 ms DMA buffering
#define I2S_OUT_WRITE_TIMEOUT_MS 1000
#define I2S_OUT_DIR_SETTLE_FRAMES 16  // direction/enable to first pulse gap
#define I2S_OUT_TAIL_FRAMES 4

static i2s_chan_handle_t i2sOutChannel = NULL;
static SemaphoreHandle_t i2sOutMutex = NULL;
// Stereo pair per frame; only touched by the stream owner (feeder task or a
// step() holding i2sOutMutex).
static uint32_t i2sOutBatch[I2S_OUT_BLOCK_FRAMES * 2];
static size_t i2sOutBatchFrames = 0;
static uint32_t i2sOutFeederBlock[I2S_OUT_BLOCK_FRAMES * 2];

static bool i2sOutWrite(const uint32_t *words, size_t frames)
{
  size_t bytesWritten = 0;
  size_t bytes = frames * 2 * sizeof(uint32_t);
  return i2s_channel_write(i2sOutChannel, words, bytes, &bytesWritten,
                           I2S_OUT_WRITE_TIMEOUT_MS) == ESP_OK &&
         bytesWritten == bytes;
}

static void i2sOutBatchFlush()
{
  if (i2sOutBatchFrames > 0)
  {
    i2sOutWrite(i2sOutBatch, i2sOutBatchFrames);
    i2sOutBatchFrames = 0;
  }
}

static void i2sOutBatchAppend(uint32_t word, long frames)
{
  while (frames > 0)
  {
    if (i2sOutBatchFrames >= I2S_OUT_BLOCK_FRAMES)
    {
      i2sOutBatchFlush();
    }
    i2sOutBatch[i2sOutBatchFrames * 2] = word;
    i2sOutBatch[(i2sOutBatchFrames * 2) + 1] = word;
    i2sOutBatchFrames++;
    frames--;
  }
}

static uint32_t shiftRegisterSnapshot()
{
  portENTER_CRITICAL(&shiftRegisterMux);
  uint32_t state = shiftRegisterState;
  portEXIT_CRITICAL(&shiftRegisterMux);
  return state;
}

// Keeps the DMA stream fed with the live state word whenever no move is being
// streamed, so plain shiftRegisterWrite() calls (enable, direction, beeper)
// reach the hardware without any dedicated transmit call. The one-tick delay
// after releasing the mutex guarantees a blocked step() acquires it promptly.
static void i2sOutFeederTask(void *unused)
{
  (void)unused;
  while (true)
  {
    if (xSemaphoreTake(i2sOutMutex, portMAX_DELAY) == pdTRUE)
    {
      uint32_t state = shiftRegisterSnapshot();
      for (size_t i = 0; i < I2S_OUT_BLOCK_FRAMES; i++)
      {
        i2sOutFeederBlock[i * 2] = state;
        i2sOutFeederBlock[(i * 2) + 1] = state;
      }
      i2sOutWrite(i2sOutFeederBlock, I2S_OUT_BLOCK_FRAMES);
      xSemaphoreGive(i2sOutMutex);
    }
    vTaskDelay(1);
  }
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
  portEXIT_CRITICAL(&shiftRegisterMux);
}

static void shiftRegisterInit()
{
  if (shiftRegisterReady)
  {
    return;
  }

  shiftRegisterState = SHIFT_REGISTER_DEFAULT_STATE;

  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = I2S_OUT_DMA_DESC_NUM;
  channelConfig.dma_frame_num = I2S_OUT_BLOCK_FRAMES;
  // On a feeder underrun, clock out zeros instead of looping stale DMA data:
  // repeating buffered step pulses would move the motor; zeros only drop the
  // enable/direction lines until the feeder catches up.
  channelConfig.auto_clear = true;
  if (i2s_new_channel(&channelConfig, &i2sOutChannel, NULL) != ESP_OK)
  {
    updateDisplayLog(langText("status_stepper_i2s_failed"));
    return;
  }

  i2s_std_config_t stdConfig = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_OUT_SAMPLE_RATE_HZ),
      // MSB (left-justified): WS edges sit exactly on word boundaries so every
      // latch captures a complete word, and bits go out MSB-first as the
      // shift-register chain expects.
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = I2S_OUT_BCK,
          .ws = I2S_OUT_WS,
          .dout = I2S_OUT_DATA,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };
  if (i2s_channel_init_std_mode(i2sOutChannel, &stdConfig) != ESP_OK)
  {
    updateDisplayLog(langText("status_stepper_i2s_failed"));
    return;
  }

  // Preload the DMA buffers with the idle state so the first clocked-out
  // frames latch the safe default instead of zeros (zeros would briefly
  // enable the motors: the disable bit is active-high).
  for (size_t i = 0; i < I2S_OUT_BLOCK_FRAMES; i++)
  {
    i2sOutFeederBlock[i * 2] = shiftRegisterState;
    i2sOutFeederBlock[(i * 2) + 1] = shiftRegisterState;
  }
  size_t preloaded = 1;
  while (preloaded > 0)
  {
    if (i2s_channel_preload_data(i2sOutChannel, i2sOutFeederBlock,
                                 sizeof(i2sOutFeederBlock), &preloaded) != ESP_OK)
    {
      break;
    }
  }

  if (i2s_channel_enable(i2sOutChannel) != ESP_OK)
  {
    updateDisplayLog(langText("status_stepper_i2s_failed"));
    return;
  }

  i2sOutMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(i2sOutFeederTask, "i2sOut", 3072, NULL, 2, NULL, 0);
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

  shiftRegisterInit();
  if (!shiftRegisterReady)
  {
    updateDisplayLog(langText("status_stepper_i2s_failed"));
    return;
  }

  StepperPins *motor = &steppers[stepperNum];
  unsigned long intervalUs = stepperPulseIntervalUs(motor->rpm);
  long framesPerStep = (long)((intervalUs + (I2S_OUT_FRAME_US / 2)) / I2S_OUT_FRAME_US);
  if (framesPerStep < 2)
  {
    framesPerStep = 2; // 1 frame pulse high + at least 1 frame low
  }

  stepperEnableAll(true);
  shiftRegisterWrite(motor->dir, HIGH);
  uint32_t stepMask = 1UL << motor->step;

  // Stream the whole move as one pulse train. i2s_channel_write() blocks while
  // the DMA buffers are full, so this paces itself to the frame clock; the
  // physical move finishes up to one DMA depth (~8 ms) after the last write.
  if (xSemaphoreTake(i2sOutMutex, portMAX_DELAY) == pdTRUE)
  {
    i2sOutBatchAppend(shiftRegisterSnapshot(), I2S_OUT_DIR_SETTLE_FRAMES);
    for (long i = 0; i < steps; i++)
    {
      // Re-read the state each step so a concurrent beeper/enable write from
      // the other core still reaches the hardware mid-move.
      uint32_t base = shiftRegisterSnapshot();
      i2sOutBatchAppend(base | stepMask, 1);
      i2sOutBatchAppend(base, framesPerStep - 1);
    }
    i2sOutBatchAppend(shiftRegisterSnapshot(), I2S_OUT_TAIL_FRAMES);
    i2sOutBatchFlush();
    xSemaphoreGive(i2sOutMutex);
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
