int profileListCounter;

void trickler_start_event_cb(lv_event_t *e)
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

void nnn_event_cb(lv_event_t *e)
{
  addWeight = 0.001;
  beep("button");
}

void nn_event_cb(lv_event_t *e)
{
  addWeight = 0.01;
  beep("button");
}

void n_event_cb(lv_event_t *e)
{
  addWeight = 0.1;
  beep("button");
}

void p_event_cb(lv_event_t *e)
{
  addWeight = 1.0;
  beep("button");
}

void add_event_cb(lv_event_t *e)
{
  config.targetWeight += addWeight;
  if (config.targetWeight > MAX_TARGET_WEIGHT)
  {
    config.targetWeight = MAX_TARGET_WEIGHT;
  }
  beep("button");
  setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
}

void sub_event_cb(lv_event_t *e)
{
  config.targetWeight -= addWeight;
  if (config.targetWeight < 0)
  {
    config.targetWeight = 0.0;
  }
  beep("button");
  setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
}

void prev_event_cb(lv_event_t *e)
{
  profileListCounter--;
  if (profileListCounter < 0)
    profileListCounter = (profileListCount - 1);
  setProfile(profileListCounter);
}

void delete_event_cb(lv_event_t *e)
{
  deleteSelectedProfile();
}

void next_event_cb(lv_event_t *e)
{
  profileListCounter++;
  if (profileListCounter > (profileListCount - 1))
    profileListCounter = 0;
  setProfile(profileListCounter);
}

