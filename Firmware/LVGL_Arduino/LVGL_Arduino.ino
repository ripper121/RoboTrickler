/*Using LVGL with Arduino requires some extra steps:
  Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */
#include <lvgl.h>
#include "ui.h"
#include <TFT_eSPI.h>
#define LCD_EN              GPIO_NUM_5

#define DISP_TASK_STACK                 4096*2
#define DISP_TASK_PRO                   2
#define DISP_TASK_CORE                  0
TaskHandle_t lv_disp_tcb = NULL;
/*Change to your screen resolution*/
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

void trickler_start(lv_event_t * e)
{
  if (String(lv_label_get_text(ui_LabelStart)) == "Stop"){
    lv_label_set_text(ui_LabelStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT );
  }
  else{
    lv_label_set_text(ui_LabelStart, "Stop");
    lv_obj_set_style_bg_color(ui_ButtonStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT );
  }
}

IRAM_ATTR void lvgl_disp_task(void *parg) { 
    while(1) {
            lv_timer_handler();
    }
}

IRAM_ATTR void disp_task_init(void) {

    xTaskCreatePinnedToCore(lvgl_disp_task,     // task
                            "lvglTask",         // name for task
                            DISP_TASK_STACK,    // size of task stack
                            NULL,               // parameters
                            DISP_TASK_PRO,      // priority
                            // nullptr,
                            &lv_disp_tcb,
                            DISP_TASK_CORE      // must run the task on same core
                                                // core
    );
}

void setup()
{
  Serial.begin( 115200 ); /* prepare for possible serial debug */
  displayInit();
  ui_init();
  disp_task_init();
  Serial.println( "Setup done" );
}

void loop()
{
  Serial.println( "Loop" );
  delay(5000);
}
