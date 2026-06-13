bool isTricklerRunning()
{
  return (tricklerState == TRICKLER_RUNNING) || (tricklerState == TRICKLER_FINISHED);
}

bool isTricklerFinished()
{
  return tricklerState == TRICKLER_FINISHED;
}

bool isCalibrationProfilePromptPending()
{
  return tricklerState == TRICKLER_CALIBRATION_PROMPT;
}

static long weightTicks(float value)
{
  return lroundf(value * WEIGHT_SCALE_FACTOR);
}

static bool weightIsZero(float value)
{
  return weightTicks(value) == 0;
}

static bool weightAtOrAbove(float value, float threshold)
{
  return weightTicks(value) >= weightTicks(threshold);
}

static bool weightAtOrBelow(float value, float threshold)
{
  return weightTicks(value) <= weightTicks(threshold);
}

static bool weightBelow(float value, float threshold)
{
  return weightTicks(value) < weightTicks(threshold);
}

void setTricklerState(TricklerState state)
{
  tricklerState = state;
}

static bool canStartFirstThrowAtCurrentWeight()
{
  if (config.profile_startAtZero)
  {
    return weightIsZero(weight);
  }
  return weightAtOrAbove(weight, 0.0f);
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

  double exactSteps = remainingUnits * ((double)MOTOR_REV_STEPS / unitsPerThrow);
  if ((exactSteps <= 0.0) || (exactSteps > 2147483647.0))
  {
    return 0;
  }

  long steps = (long)exactSteps;
  if (outUnits != NULL)
  {
    *outUnits = ((double)steps * unitsPerThrow) / (double)MOTOR_REV_STEPS;
  }
  return steps;
}

static bool runBulkStepperMove(String &infoText)
{
  // The optional bulk move removes most remaining weight first; profile steps
  // then handle the fine approach to target.
  double remainingUnits = (double)config.targetWeight - (double)weight - (double)config.profile_weightGap;
  if (remainingUnits <= 0.0)
  {
    return true;
  }

  int stepperNum = config.profile_bulkActuator;
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    return false;
  }

  if (!config.profile_stepperEnabled[stepperNum])
  {
    return true;
  }

  int speed = config.profile_stepperUnitsPerThrowSpeed[stepperNum];
  if (speed <= 0)
  {
    speed = 200;
  }

  double units = 0.0;
  long stepsToMove = calculateStepperStepsForUnits(remainingUnits, config.profile_stepperUnitsPerThrow[stepperNum], &units);
  if (stepsToMove <= 0)
  {
    return true;
  }

  setStepperSpeed(stepperNum, speed);
  step(stepperNum, stepsToMove);
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

  return true;
}

static int stepperSpeedOld[3] = {0, 0, 0};

static void setStepperSpeedIfChanged(byte stepperNum, int speed)
{
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    return;
  }
  if (stepperSpeedOld[stepperNum] != speed)
  {
    setStepperSpeed(stepperNum, speed);
    stepperSpeedOld[stepperNum] = speed;
  }
}

void startCalibrationProfilePrompt()
{
  stopTrickler();
  setTricklerState(TRICKLER_CALIBRATION_PROMPT);
  calibrationProfilePromptTime = millis();
  measurementCount = config.profile_generalMeasurements;
  weightCounter = 0;
  newData = false;
}

void handleCalibrationProfilePrompt()
{
  // After running the calibration throw, wait for a fresh scale read before
  // offering to generate a powder profile from the measured weight.
  if (!isCalibrationProfilePromptPending())
  {
    return;
  }

  readWeight();

  if (newData && (lastScaleWeightReadTime > calibrationProfilePromptTime))
  {
    setTricklerState(TRICKLER_IDLE);
    newData = false;
    weightCounter = 0;
    if (confirmBox(String(langText("msg_create_profile_prompt")) + String(weight, 3) + " gn", UI_FONT_NORMAL, lv_color_hex(0xFFFFFF)))
    {
      String profileName = "";
      if (createProfileFromCalibration(weight, profileName))
      {
        messageBox(String(langText("msg_profile_created")) + profileName + ".txt", UI_FONT_NORMAL, lv_color_hex(0x00FF00), true);
      }
      else
      {
        messageBox(langText("msg_create_profile_failed"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), true);
      }
    }
  }
}

static bool isCalibrationProfile()
{
  return strcmp(config.profile, "calibrate") == 0;
}

static void handleOverTrickle()
{
  setLabelTextColor(ui_LabelTricklerWeight, 0xFF0000);
  beep("done");
  delay(250);
  beep("done");
  delay(250);
  beep("done");
  String messageText = langText("msg_over_trickle");
  if (config.profile_trickleCounter)
  {
    messageText += String(trickleCounter);
  }
  stopTrickler();
  messageBox(messageText.c_str(), UI_FONT_LARGE, lv_color_hex(0xFF0000), true);
}

static void handleTargetReached(bool weightWithinTolerance)
{
  setLabelTextColor(ui_LabelTricklerWeight, weightWithinTolerance ? 0x00FF00 : 0xFFFF00);

  if ((tricklerState == TRICKLER_RUNNING) &&
      weightAtOrAbove(weight, config.targetWeight + config.profile_alarmThreshold) &&
      (config.profile_alarmThreshold > 0))
  {
    handleOverTrickle();
    measurementCount = 0;
    return;
  }

  if (!isTricklerFinished())
  {
    beep("done");
    if (config.trickleCounter)
    {
      config.trickleCount++;
    }
    if (config.profile_trickleCounter && weightWithinTolerance)
    {
      trickleCounter++;
      updateDisplayLog(String(langText("status_done")) + langText("status_count") + String(trickleCounter), true);
    }
    else
    {
      updateDisplayLog(langText("status_done"), true);
    }
  }
  measurementCount = 0;
  setTricklerState(TRICKLER_FINISHED);
}

static int selectProfileStep()
{
  int profileStep = config.profile_count - 1;
  for (int i = 0; i < config.profile_count; i++)
  {
    if (weight <= (config.targetWeight - config.profile_weight[i]))
    {
      profileStep = i;
      break;
    }
  }
  return profileStep;
}

void updateActiveProfileStepCounterDisplay(int actualWeightCounter)
{
  if ((activeProfileStep < 0) ||
      (activeProfileStep >= config.profile_count) ||
      (tricklerState != TRICKLER_RUNNING) ||
      (measurementCount != config.profile_measurements[activeProfileStep]))
  {
    return;
  }

  char infoLine[64];
  snprintf(infoLine,
           sizeof(infoLine),
           "W%.3f ST%ld SP%d M%d/%d",
           config.profile_weight[activeProfileStep],
           config.profile_steps[activeProfileStep],
           config.profile_speed[activeProfileStep],
           config.profile_measurements[activeProfileStep],
           actualWeightCounter);
  updateDisplayLog(infoLine, true);
}

static bool runProfileStep(bool calibrationProfile, int actualWeightCounter)
{
  if (config.profile_count <= 0)
  {
    updateDisplayLog(langText("status_invalid_profile"), true);
    stopTrickler();
    return false;
  }

  int profileStep = selectProfileStep();
  activeProfileStep = profileStep;
  byte stepperNum = config.profile_num[profileStep];
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    updateDisplayLog(langText("status_invalid_stepper"), true);
    stopTrickler();
    return false;
  }

  setStepperSpeedIfChanged(stepperNum, config.profile_speed[profileStep]);
  step(stepperNum, config.profile_steps[profileStep]);

  measurementCount = config.profile_measurements[profileStep];

  if (calibrationProfile)
  {
    startCalibrationProfilePrompt();
    return true;
  }

  updateActiveProfileStepCounterDisplay(actualWeightCounter);
  return true;
}

static void handleProfileRunning(bool calibrationProfile, int actualWeightCounter)
{
  if (weightBelow(weight, 0.0f) || (firstProfileMovePending && !canStartFirstThrowAtCurrentWeight()))
  {
    if (config.profile_startAtZero)
    {
      updateDisplayLog(langText("status_waiting_zero"), true);
    }
    return;
  }

  updateDisplayLog(langText("status_running"), true);
  setLabelTextColor(ui_LabelTricklerWeight, 0xFFFFFF);

  DEBUG_PRINTLN("Profile Running");
  if (firstProfileMovePending)
  {
    firstProfileMovePending = false;
    String infoText = "";
    infoText.reserve(48);
    if (!runBulkStepperMove(infoText))
    {
      updateDisplayLog(langText("status_bulk_failed"), true);
      stopTrickler();
      return;
    }
    if (infoText.length() > 0)
    {
      if (calibrationProfile)
      {
        startCalibrationProfilePrompt();
        return;
      }
      measurementCount = config.profile_generalMeasurements;
      updateDisplayLog(infoText, true);
      return;
    }
  }

  runProfileStep(calibrationProfile, actualWeightCounter);
}

static void handleNewWeight()
{
  // Weight reads drive the state machine: finish on target, otherwise choose
  // the next profile step and motor movement.
  newData = false;
  int actualWeightCounter = weightCounter;
  weightCounter = 0;

  if (weightAtOrBelow(weight, 0.0f))
  {
    firstProfileMovePending = true;
    measurementCount = config.profile_generalMeasurements;
  }

  bool calibrationProfile = isCalibrationProfile();

  DEBUG_PRINT("Weight: ");
  DEBUG_PRINTLN(weight);
  DEBUG_PRINT("TargetWeight: ");
  DEBUG_PRINTLN(String((config.targetWeight - config.profile_tolerance), 5));
  DEBUG_PRINTLN(String((config.targetWeight + config.profile_tolerance), 5));
  DEBUG_PRINT("profile_alarmThreshold: ");
  DEBUG_PRINTLN(String((config.targetWeight + config.profile_alarmThreshold), 5));

  if (!calibrationProfile &&
      weightAtOrAbove(weight, config.targetWeight - config.profile_tolerance) &&
      weightAtOrAbove(weight, 0.0f))
  {
    handleTargetReached(weightAtOrBelow(weight, config.targetWeight + config.profile_tolerance));
  }

  if (tricklerState == TRICKLER_RUNNING)
  {
    handleProfileRunning(calibrationProfile, actualWeightCounter);
  }

  if (weightAtOrBelow(weight, 0.0f) && isTricklerFinished())
  {
    updateDisplayLog(langText("status_ready"), true);
    setTricklerState(TRICKLER_RUNNING);
  }
}

void handleRunningTrickler()
{
  readWeight();
  if (newData)
  {
    handleNewWeight();
  }
}

void handleIdleWeightRead(uint32_t &readWeightTime)
{
  handleCalibrationProfilePrompt();

  if (millis() - readWeightTime >= IDLE_SCALE_READ_INTERVAL)
  {
    readWeightTime = millis();
    updateDisplayLog(langText("status_get_weight"), true);
    readWeight();
  }
}

