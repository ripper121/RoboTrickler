void disableTouchGestures()
{
  if (lvglLock())
  {
    lv_obj_t *tabViewContent = lv_tabview_get_content(ui_TabView);
    lv_obj_clear_flag(tabViewContent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tabViewContent, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(tabViewContent, LV_SCROLLBAR_MODE_OFF);
    lvglUnlock();
  }
}

#if LV_USE_LOG != 0
/* Serial debugging */
void lvglPrint(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

static uint32_t lvglMillis(void)
{
  return millis();
}

IRAM_ATTR void initDisplayTask(void)
{
  if (lvglMutex == NULL)
  {
    lvglMutex = xSemaphoreCreateRecursiveMutex();
  }

  xTaskCreatePinnedToCore(lvglDisplayTask,
                          "lvglTask",
                          DISP_TASK_STACK,
                          NULL,
                          DISP_TASK_PRO,
                          &lv_disp_tcb,
                          DISP_TASK_CORE);
}

IRAM_ATTR void lvglDisplayTask(void *parg)
{
  while (1)
  {
    if (lvglLock())
    {
      lv_timer_handler();
      lvglUnlock();
    }
    handleWebServerClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

/* Display flushing */
void lvglDisplayFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

/*Read the touchpad*/
void lvglTouchpadRead(lv_indev_t *indev, lv_indev_data_t *data)
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

void initDisplay()
{

  lv_init();
  lv_tick_set_cb(lvglMillis);

#if LV_USE_LOG != 0
  lv_log_register_print_cb(lvglPrint); /* register print function for debugging */
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
  lv_display_set_flush_cb(disp, lvglDisplayFlush);
  lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  /*Initialize the (dummy) input device driver*/
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvglTouchpadRead);
  lv_indev_set_display(indev, disp);
}
