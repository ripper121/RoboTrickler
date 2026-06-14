float addWeight = 0.1;
byte addWeightIndex = 2;

void updateAddWeightLabel()
{
  if (addWeightIndex >= WEIGHT_STEP_COUNT)
  {
    addWeightIndex = 0;
  }
  addWeight = WEIGHT_STEP_SIZES[addWeightIndex];
  if (ui_LabelAddWeightCycle != NULL)
  {
    setLabelText(ui_LabelAddWeightCycle, String(addWeight, 3).c_str());
  }
}

void setAddWeight(byte index)
{
  addWeightIndex = index;
  updateAddWeightLabel();
}

void toggleTrickler_event_cb(lv_event_t *e)
{
  if (!isTricklerRunning())
  {
    DEBUG_PRINTLN("TricklerStart");
    startTrickler();
  }
  else
  {
    DEBUG_PRINTLN("TricklerStop");
    stopTrickler();
  }
}

void setAddWeightFine_event_cb(lv_event_t *e)
{
  setAddWeight(0);
  beep("button");
}

void cycleAddWeight_event_cb(lv_event_t *e)
{
  // updateAddWeightLabel() wraps the index past WEIGHT_STEP_COUNT itself.
  addWeightIndex++;
  updateAddWeightLabel();
  beep("button");
}

void setAddWeightMedium_event_cb(lv_event_t *e)
{
  setAddWeight(2);
  beep("button");
}

void setAddWeightLarge_event_cb(lv_event_t *e)
{
  setAddWeight(3);
  beep("button");
}

void increaseTargetWeight_event_cb(lv_event_t *e)
{
  config.targetWeight += addWeight;
  if (config.targetWeight > MAX_TARGET_WEIGHT)
  {
    config.targetWeight = MAX_TARGET_WEIGHT;
  }
  beep("button");
  updateTargetWeightLabel();
}

void decreaseTargetWeight_event_cb(lv_event_t *e)
{
  config.targetWeight -= addWeight;
  if (config.targetWeight < 0)
  {
    config.targetWeight = 0.0;
  }
  beep("button");
  updateTargetWeightLabel();
}

void selectPreviousProfile_event_cb(lv_event_t *e)
{
  profileListCounter--;
  if (profileListCounter < 0)
    profileListCounter = (profileListCount - 1);
  setProfile(profileListCounter);
}

void openProfileTune_event_cb(lv_event_t *e)
{
  tuneSelectedProfile();
}

void requestProfileDelete_event_cb(lv_event_t *e)
{
  deleteSelectedProfile();
}

void selectNextProfile_event_cb(lv_event_t *e)
{
  profileListCounter++;
  if (profileListCounter > (profileListCount - 1))
    profileListCounter = 0;
  setProfile(profileListCounter);
}
