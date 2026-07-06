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

// Weight comparisons run directly on floats. IEEE-754 ordering is already exact,
// so >=/<=/< need no scaling; the only care is that two values which would render
// identically (within half a WEIGHT_RESOLUTION step) count as equal. fabsf() gives
// that tolerance, and isgreaterequal()/islessequal()/isless() are the standard
// NaN-quiet <math.h> comparison macros (a NaN weight compares false, never traps),
// which is exactly why the old scale-to-integer trick is unnecessary.
static bool weightsEqual(float a, float b)
{
  return fabsf(a - b) < WEIGHT_EPSILON;
}

static bool weightIsZero(float value)
{
  return weightsEqual(value, 0.0f);
}

static bool weightAtOrAbove(float value, float threshold)
{
  return isgreaterequal(value, threshold - WEIGHT_EPSILON);
}

static bool weightAtOrBelow(float value, float threshold)
{
  return islessequal(value, threshold + WEIGHT_EPSILON);
}

static bool weightBelow(float value, float threshold)
{
  return isless(value, threshold - WEIGHT_EPSILON);
}

void setTricklerState(TricklerState state)
{
  tricklerState = state;
}

static bool canStartFirstThrowAtCurrentWeight()
{
  if (config.profileStartAtZero)
  {
    return weightIsZero(weight);
  }
  return weightAtOrAbove(weight, 0.0f);
}

static long calculateStepperStepsForWeight(double remainingWeight, double weightPerRev, double *outWeight)
{
  if (outWeight != NULL)
  {
    *outWeight = 0.0;
  }
  if ((remainingWeight <= 0.0) || (weightPerRev <= 0.0))
  {
    return 0;
  }

  double exactSteps = remainingWeight * ((double)config.motorStepsPerRev / weightPerRev);
  if ((exactSteps <= 0.0) || (exactSteps > 2147483647.0))
  {
    return 0;
  }

  long steps = (long)exactSteps;
  if (outWeight != NULL)
  {
    *outWeight = ((double)steps * weightPerRev) / (double)config.motorStepsPerRev;
  }
  return steps;
}

static bool runBulkStepperMove(String &infoText)
{
  // The optional bulk move removes most remaining weight first; profile steps
  // then handle the fine approach to target.
  double remainingWeight = (double)config.targetWeight - (double)weight - (double)config.profileWeightGap;
  if (remainingWeight <= 0.0)
  {
    return true;
  }

  int stepperNum = config.profileBulkStepper;
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    return false;
  }

  if (!config.profileStepperEnabled[stepperNum])
  {
    return true;
  }

  int rpm = config.profileStepperRpm[stepperNum];
  if (rpm <= 0)
  {
    rpm = 200;
  }

  double dispensedWeight = 0.0;
  long stepsToMove = calculateStepperStepsForWeight(remainingWeight, config.profileStepperWeightPerRev[stepperNum], &dispensedWeight);
  if (stepsToMove <= 0)
  {
    return true;
  }

  setStepperRpm(stepperNum, rpm);
  step(stepperNum, stepsToMove);
  remainingWeight -= dispensedWeight;
  if (remainingWeight < 0.0)
  {
    remainingWeight = 0.0;
  }

  infoText += "B";
  infoText += String(stepperNum);
  infoText += " ST";
  infoText += String(stepsToMove);
  infoText += " ";

  return true;
}

void startCalibrationProfilePrompt()
{
  stopTrickler();
  setTricklerState(TRICKLER_CALIBRATION_PROMPT);
  calibrationProfilePromptTime = millis();
  measurementCount = config.profileGeneralMeasurements;
  weightCounter = 0;
  newWeightData = false;
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

  if (newWeightData && (lastScaleWeightReadTime > calibrationProfilePromptTime))
  {
    setTricklerState(TRICKLER_IDLE);
    newWeightData = false;
    weightCounter = 0;
    if (confirmBox(String(langText("msg_create_profile_prompt")) + weightToString(weight) + " gn", UI_FONT_NORMAL, lv_color_hex(0xFFFFFF)))
    {
      String profileName = "";
      if (createProfileFromCalibration(weight, profileName))
      {
        successBox(String(langText("msg_profile_created")) + profileName + ".txt", true);
      }
      else
      {
        errorBox(langText("msg_create_profile_failed"), true);
      }
    }
  }
}

static bool isCalibrationProfile()
{
  return strcmp(config.profileName, "calibrate") == 0;
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
  if (config.profileSessionCounter)
  {
    messageText += String(sessionCount);
  }
  stopTrickler();
  messageBox(messageText.c_str(), UI_FONT_LARGE, lv_color_hex(0xFF0000), true);
}

static void handleTargetReached(bool weightWithinTolerance)
{
  setLabelTextColor(ui_LabelTricklerWeight, weightWithinTolerance ? 0x00FF00 : 0xFFFF00);

  if ((tricklerState == TRICKLER_RUNNING) &&
      weightAtOrAbove(weight, config.targetWeight + config.profileAlarmThreshold) &&
      (config.profileAlarmThreshold > 0))
  {
    handleOverTrickle();
    measurementCount = 0;
    return;
  }

  if (!isTricklerFinished())
  {
    beep("done");
    if (config.totalCounterEnable)
    {
      config.totalCount++;
    }
    if (config.profileSessionCounter && weightWithinTolerance)
    {
      sessionCount++;
      updateDisplayLog(String(langText("status_done")) + langText("status_count") + String(sessionCount), true);
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
  int profileStep = config.profileEntryCount - 1;
  for (int i = 0; i < config.profileEntryCount; i++)
  {
    if (weightAtOrBelow(weight, config.targetWeight - config.profileDiffWeight[i]))
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
      (activeProfileStep >= config.profileEntryCount) ||
      (tricklerState != TRICKLER_RUNNING) ||
      (measurementCount != config.profileMeasurements[activeProfileStep]))
  {
    return;
  }

  char infoLine[64];
  snprintf(infoLine,
           sizeof(infoLine),
           "W%.*f ST%ld RPM%d M%d/%d",
           WEIGHT_DECIMALS,
           config.profileDiffWeight[activeProfileStep],
           config.profileSteps[activeProfileStep],
           config.profileRpm[activeProfileStep],
           config.profileMeasurements[activeProfileStep],
           actualWeightCounter);
  updateDisplayLog(infoLine, true);
}

static bool runProfileStep(bool calibrationProfile, int actualWeightCounter)
{
  if (config.profileEntryCount <= 0)
  {
    updateDisplayLog(langText("status_invalid_profile"), true);
    stopTrickler();
    return false;
  }

  int profileStep = selectProfileStep();
  activeProfileStep = profileStep;
  byte stepperNum = config.profileStepper[profileStep];
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    updateDisplayLog(langText("status_invalid_stepper"), true);
    stopTrickler();
    return false;
  }

  // setStepperRpm() is a plain assignment that step() reads, so set it every
  // step. A cached "if changed" guard would go stale after the bulk move (which
  // calls setStepperRpm directly) and could run a fine step at the bulk RPM.
  setStepperRpm(stepperNum, config.profileRpm[profileStep]);
  step(stepperNum, config.profileSteps[profileStep]);

  measurementCount = config.profileMeasurements[profileStep];

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
    if (config.profileStartAtZero)
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
      measurementCount = config.profileGeneralMeasurements;
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
  newWeightData = false;
  int actualWeightCounter = weightCounter;
  weightCounter = 0;

  if (weightAtOrBelow(weight, 0.0f))
  {
    firstProfileMovePending = true;
    measurementCount = config.profileGeneralMeasurements;
  }

  bool calibrationProfile = isCalibrationProfile();

  DEBUG_PRINT("Weight: ");
  DEBUG_PRINTLN(weight);
  DEBUG_PRINT("TargetWeight: ");
  DEBUG_PRINTLN(String((config.targetWeight - config.profileTolerance), 5));
  DEBUG_PRINTLN(String((config.targetWeight + config.profileTolerance), 5));
  DEBUG_PRINT("profileAlarmThreshold: ");
  DEBUG_PRINTLN(String((config.targetWeight + config.profileAlarmThreshold), 5));

  if (!calibrationProfile &&
      weightAtOrAbove(weight, config.targetWeight - config.profileTolerance) &&
      weightAtOrAbove(weight, 0.0f))
  {
    handleTargetReached(weightAtOrBelow(weight, config.targetWeight + config.profileTolerance));
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
  if (newWeightData)
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
