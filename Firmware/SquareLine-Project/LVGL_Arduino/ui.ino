#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
  tft.endWrite();

  lv_disp_flush_ready( disp_drv );
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
  uint16_t touchX, touchY;

  bool touched = tft.getTouch( &touchX, &touchY, 1);

  if ( !touched )
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

void displayInit() {

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, LOW);

  tft.init();
  tft.setRotation(0);
  tft.initDMA();

  /*Set the touchscreen calibration data,
    the actual data for your display can be acquired using
    the Generic -> Touch_calibrate example from the TFT_eSPI library*/
  uint16_t calData[5] = { 248, 3571, 202, 3647, 5};
  tft.setTouch( calData );

  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );
}
