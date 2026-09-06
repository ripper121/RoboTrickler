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
    formatWeight(text, sizeof(text), addWeight);
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

// The Core 1 trickler state machine reads config.targetWeight live while a run is
// active, so an on-screen edit from the Core 0 display task would race that reader
// and change the target of an in-progress throw. Refuse edits while running, the
// same way the web API does in handleSetTarget().
static bool targetWeightEditAllowed()
{
  return !isTricklerRunning();
}

void increaseTargetWeight_event_cb(lv_event_t *e)
{
  if (!targetWeightEditAllowed())
  {
    return;
  }
  config.targetWeight = clampWeight(config.targetWeight + addWeight);
  beep("button");
  updateTargetWeightLabel();
}

void decreaseTargetWeight_event_cb(lv_event_t *e)
{
  if (!targetWeightEditAllowed())
  {
    return;
  }
  config.targetWeight = clampWeight(config.targetWeight - addWeight);
  beep("button");
  updateTargetWeightLabel();
}

void selectPreviousProfile_event_cb(lv_event_t *e)
{
  // Nothing to select (and wrapping to profileListCount - 1 would be -1) when the
  // list is empty, so skip the pointless loadProfile()/saveConfiguration() cycle.
  if (profileListCount <= 0)
  {
    return;
  }
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
  if (profileListCount <= 0)
  {
    return;
  }
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
