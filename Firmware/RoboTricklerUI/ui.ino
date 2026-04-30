int profileListCounter;
volatile bool messageBoxOpen = false;
volatile bool confirmBoxOpen = false;
volatile bool confirmBoxResult = false;
lv_obj_t *ui_ButtonMessageNo = NULL;
lv_obj_t *ui_LabelButtonMessageNo = NULL;

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

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

void next_event_cb(lv_event_t *e)
{
  profileListCounter++;
  if (profileListCounter > (profileListCount - 1))
    profileListCounter = 0;
  setProfile(profileListCounter);
}

void message_event_cb(lv_event_t *e)
{
  if (confirmBoxOpen)
  {
    confirmBoxResult = true;
  }
  if (lvglLock())
  {
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    messageBoxOpen = false;
    confirmBoxOpen = false;
    lvglUnlock();
  }
  if (restart_now)
  {
    delay(1000);
    ESP.restart();
  }
}

void confirm_no_event_cb(lv_event_t *e)
{
  confirmBoxResult = false;
  if (lvglLock())
  {
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    messageBoxOpen = false;
    confirmBoxOpen = false;
    lvglUnlock();
  }
}

void messageBox(String message, const lv_font_t *font, lv_color_t color, bool wait)
{
  if (lvglLock())
  {
    if (ui_ButtonMessageNo != NULL)
    {
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_x(ui_ButtonMessage, 0);
    lv_label_set_text(ui_LabelButtonMessage, "OK");
    messageBoxOpen = true;
    lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelMessages, message.c_str());
    lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    lvglUnlock();
  }
  if (wait)
  {
    while (messageBoxOpen)
    {
      if (lvglLock())
      {
        lv_timer_handler();
        lvglUnlock();
      }
      delay(10);
    }
  }
}

bool confirmBox(String message, const lv_font_t *font, lv_color_t color)
{
  confirmBoxResult = false;
  if (lvglLock())
  {
    if (ui_ButtonMessageNo == NULL)
    {
      ui_ButtonMessageNo = lv_btn_create(ui_PanelMessages);
      lv_obj_set_width(ui_ButtonMessageNo, 100);
      lv_obj_set_height(ui_ButtonMessageNo, 50);
      lv_obj_set_x(ui_ButtonMessageNo, 70);
      lv_obj_set_y(ui_ButtonMessageNo, 45);
      lv_obj_set_align(ui_ButtonMessageNo, LV_ALIGN_CENTER);
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
      lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_SCROLLABLE);

      ui_LabelButtonMessageNo = lv_label_create(ui_ButtonMessageNo);
      lv_obj_set_width(ui_LabelButtonMessageNo, LV_SIZE_CONTENT);
      lv_obj_set_height(ui_LabelButtonMessageNo, LV_SIZE_CONTENT);
      lv_obj_set_align(ui_LabelButtonMessageNo, LV_ALIGN_CENTER);
      lv_label_set_text(ui_LabelButtonMessageNo, "No");
      lv_obj_add_event_cb(ui_ButtonMessageNo, confirm_no_event_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_set_x(ui_ButtonMessage, -70);
    lv_label_set_text(ui_LabelButtonMessage, "Yes");
    lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    messageBoxOpen = true;
    confirmBoxOpen = true;
    lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelMessages, message.c_str());
    lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    lvglUnlock();
  }

  while (messageBoxOpen)
  {
    if (lvglLock())
    {
      lv_timer_handler();
      lvglUnlock();
    }
    delay(10);
  }

  if (lvglLock())
  {
    if (ui_ButtonMessageNo != NULL)
    {
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_x(ui_ButtonMessage, 0);
    lv_label_set_text(ui_LabelButtonMessage, "OK");
    lvglUnlock();
  }

  return confirmBoxResult;
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp_drv);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;

  bool touched = tft.getTouch(&touchX, &touchY, 1);

  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

void displayInit()
{

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, LOW);

  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  tft.initDMA();

  /*Set the touchscreen calibration data,
    the actual data for your display can be acquired using
    the Generic -> Touch_calibrate example from the TFT_eSPI library*/
  uint16_t calData[5] = TOUCH_CAL_DATA;
  tft.setTouch(calData);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX / 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = LV_HOR_RES_MAX;
  disp_drv.ver_res = LV_VER_RES_MAX;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
}
