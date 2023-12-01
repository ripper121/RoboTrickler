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
  if (String(lv_label_get_text(ui_LabelTricklerStart)) == "Start")
  {
    DEBUG_PRINTLN("TricklerStart");
    stopLogger();
    startTrickler();
  }
  else
  {
    DEBUG_PRINTLN("TricklerStop");
    stopTrickler();
  }
}

void logger_start_event_cb(lv_event_t *e)
{
  if (String(lv_label_get_text(ui_LabelLoggerStart)) == "Start")
  {
    DEBUG_PRINTLN("LoggerStart");
    stopTrickler();
    startLogger();
  }
  else
  {
    DEBUG_PRINTLN("LoggerStop");
    stopLogger();
  }
}

void startMeasurment()
{
  running = true;
  finished = false;
  beep(button);
}

void stopMeasurment()
{
  running = false;
  finished = true;
  beep(button);
}

void startTrickler()
{
  config.mode = trickler;
  lv_label_set_text(ui_LabelTricklerStart, "Stop");
  lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

  String tempProfile = String(config.profile);
  char buffer[64];
  lv_roller_get_selected_str(ui_RollerProfile, config.profile, sizeof(config.profile));

  if (tempProfile != String(config.profile))
  {
    readProfile(String("/" + String(config.profile) + ".txt").c_str(), config);
    saveConfiguration("/config.txt", config);
  }
  updateDisplayLog(String("Profile: " + String(config.profile) + " selected!"));
  
  startMeasurment();
}

void stopTrickler()
{
  stopMeasurment();
  lv_label_set_text(ui_LabelTricklerStart, "Start");
  lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui_LabelTricklerWeight,"-.-");
  lv_label_set_text(ui_LabelInfo, "");
  lv_label_set_text(ui_LabelLoggerInfo, "");
}

void startLogger()
{
  config.mode = logger;
  lv_label_set_text(ui_LabelLoggerStart, "Stop");
  lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
  startMeasurment();
}

void stopLogger()
{
  stopMeasurment();
  lv_label_set_text(ui_LabelLoggerStart, "Start");
  lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui_LabelLoggerWeight,"-.-");
  lv_label_set_text(ui_LabelInfo, "");
  lv_label_set_text(ui_LabelLoggerInfo, "");
}

void nnn_event_cb(lv_event_t *e)
{
  addWeight = 0.001;
  beep(button);
}

void nn_event_cb(lv_event_t *e)
{
  addWeight = 0.01;
  beep(button);
}

void n_event_cb(lv_event_t *e)
{
  addWeight = 0.1;
  beep(button);
}

void p_event_cb(lv_event_t *e)
{
  addWeight = 1.0;
  beep(button);
}

void add_event_cb(lv_event_t *e)
{
  targetWeight += addWeight;
  if (targetWeight > MAX_TARGET_WEIGHT)
  {
    targetWeight = MAX_TARGET_WEIGHT;
  }
  beep(button);
  lv_label_set_text(ui_LabelTarget, String(targetWeight, 3).c_str());
}

void sub_event_cb(lv_event_t *e)
{
  targetWeight -= addWeight;
  if (targetWeight < 0)
  {
    targetWeight = 0.0;
  }
  beep(button);
  lv_label_set_text(ui_LabelTarget, String(targetWeight, 3).c_str());
}

void profile_event_cb(lv_event_t *e)
{
}

void message_event_cb(lv_event_t *e)
{
  lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
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

// Helper function to find the index of an option
static int find_option_index(lv_obj_t *roller, const char *desired_option)
{
  const char *options = lv_roller_get_options(roller);
  int index = 0;
  const char *current_option = options;

  while (current_option)
  {
    // Check if the current option matches the desired option
    if (strncmp(current_option, desired_option, strlen(desired_option)) == 0 &&
        (current_option[strlen(desired_option)] == '\n' || current_option[strlen(desired_option)] == '\0'))
    {
      return index;
    }
    // Move to the next option
    current_option = strchr(current_option, '\n');
    if (current_option)
    {
      current_option++; // Skip the newline character
    }
    index++;
  }
  return -1; // Option not found
}

// Function to set the roller's selected option based on a string
void set_roller_selected_str(lv_obj_t *roller, const char *option_str)
{
  int index = find_option_index(roller, option_str);
  if (index != -1)
  {
    lv_roller_set_selected(roller, index, LV_ANIM_ON);
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
  tft.setRotation(0);
  tft.initDMA();

  /*Set the touchscreen calibration data,
    the actual data for your display can be acquired using
    the Generic -> Touch_calibrate example from the TFT_eSPI library*/
  uint16_t calData[5] = {248, 3571, 202, 3647, 5};
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
