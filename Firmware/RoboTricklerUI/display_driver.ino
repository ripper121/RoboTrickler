#if LV_USE_LOG != 0
/* Serial debugging */
void lvglLogPrint(const char *message)
{
  Serial.printf(message);
  Serial.flush();
}
#endif

static uint32_t lvglMillis(void)
{
  return millis();
}

/* LVGL flush callback: copy the rendered RGB565 strip to TFT_eSPI. */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();

#if ENABLE_SCREENSHOT
  screenshotCaptureFlush(area, px_map);
#endif

  lv_display_flush_ready(disp);

  // A full refresh is split into many small strips. Let IDLE0 run between
  // strips so software-rendered labels cannot trip the CPU 0 task watchdog.
  vTaskDelay(1);
}

/* Touch callback used by LVGL's pointer input device. */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  (void)indev;
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
  lv_tick_set_cb(lvglMillis);

#if LV_USE_LOG != 0
  lv_log_register_print_cb(lvglLogPrint); /* register print function for debugging */
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

  /*Initialize the display*/
  lv_display_t *disp = lv_display_create(LV_HOR_RES_MAX, LV_VER_RES_MAX);
  lv_display_set_flush_cb(disp, my_disp_flush);
#if ENABLE_SCREENSHOT
  lv_display_add_event_cb(disp, screenshotExpandInvalidatedArea, LV_EVENT_INVALIDATE_AREA, NULL);
#endif
  lv_display_set_buffers(disp, displayDrawBuffer, NULL, sizeof(displayDrawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

  /*Initialize the (dummy) input device driver*/
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  lv_indev_set_display(indev, disp);
}
