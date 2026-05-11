void trickler_start_event_cb(lv_event_t *e)
{
  if (!running)
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

static void setTargetStep(float stepSize)
{
  addWeight = stepSize;
  playConfiguredBeep("button");
}

static void adjustTargetWeight(float delta)
{
  config.targetWeight += delta;
  if (config.targetWeight > MAX_TARGET_WEIGHT)
  {
    config.targetWeight = MAX_TARGET_WEIGHT;
  }
  if (config.targetWeight < 0)
  {
    config.targetWeight = 0.0;
  }
  playConfiguredBeep("button");
  setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
}

static void selectProfileOffset(int offset)
{
  profileListCounter += offset;
  if (profileListCounter < 0)
    profileListCounter = (profileListCount - 1);
  if (profileListCounter > (profileListCount - 1))
    profileListCounter = 0;
  selectProfile(profileListCounter);
}

void nnn_event_cb(lv_event_t *e)
{
  setTargetStep(0.001);
}

void nn_event_cb(lv_event_t *e)
{
  setTargetStep(0.01);
}

void n_event_cb(lv_event_t *e)
{
  setTargetStep(0.1);
}

void p_event_cb(lv_event_t *e)
{
  setTargetStep(1.0);
}

void add_event_cb(lv_event_t *e)
{
  adjustTargetWeight(addWeight);
}

void sub_event_cb(lv_event_t *e)
{
  adjustTargetWeight(-addWeight);
}

void prev_event_cb(lv_event_t *e)
{
  selectProfileOffset(-1);
}

void next_event_cb(lv_event_t *e)
{
  selectProfileOffset(1);
}
