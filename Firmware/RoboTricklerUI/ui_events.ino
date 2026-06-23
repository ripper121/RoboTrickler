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
    char text[16];
    snprintf(text, sizeof(text), "%.3f", addWeight);
    setLabelText(ui_LabelAddWeightCycle, text);
  }
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

void cycleAddWeight_event_cb(lv_event_t *e)
{
  // updateAddWeightLabel() wraps the index past WEIGHT_STEP_COUNT itself.
  addWeightIndex++;
  updateAddWeightLabel();
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
  selectedProfileIndex--;
  if (selectedProfileIndex < 0)
    selectedProfileIndex = (profileListCount - 1);
  setProfile(selectedProfileIndex);
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
  selectedProfileIndex++;
  if (selectedProfileIndex > (profileListCount - 1))
    selectedProfileIndex = 0;
  setProfile(selectedProfileIndex);
}

void updateScaleProtocolButtonLabel()
{
  if (ui_LabelScaleProtocol == NULL)
  {
    return;
  }

  char text[48];
  snprintf(text, sizeof(text), "%s: %s", langText("label_scale"), scaleProtocolDisplayName(config.scaleProtocol));
  setLabelText(ui_LabelScaleProtocol, text);
}

void cycleScaleProtocol_event_cb(lv_event_t *e)
{
  strlcpy(config.scaleProtocol, nextScaleProtocol(config.scaleProtocol), sizeof(config.scaleProtocol));
  serialFlush();
  saveConfiguration("/config.txt", config);
  updateScaleProtocolButtonLabel();
  beep("button");
}

void toggleWifi_event_cb(lv_event_t *e)
{
  (void)e;
  config.wifiEnabled = !config.wifiEnabled;
  saveConfiguration("/config.txt", config);
  applyWifiEnabled();
  beep("button");
}
