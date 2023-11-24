#include <lvgl.h>
#include "ui.h"
#include <TFT_eSPI.h>
#define LCD_EN GPIO_NUM_5

#define DISP_TASK_STACK 4096 * 2
#define DISP_TASK_PRO 2
#define DISP_TASK_CORE 0
TaskHandle_t lv_disp_tcb = NULL;
/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

void trickler_start_event_cb(lv_event_t *e)
{
  if (String(lv_label_get_text(ui_LabelTricklerStart)) == "Stop")
  {
    lv_label_set_text(ui_LabelTricklerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  else
  {
    lv_label_set_text(ui_LabelTricklerStart, "Stop");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelLoggerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void logger_start_event_cb(lv_event_t *e)
{
  if (String(lv_label_get_text(ui_LabelLoggerStart)) == "Stop")
  {
    lv_label_set_text(ui_LabelLoggerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  else
  {
    lv_label_set_text(ui_LabelLoggerStart, "Stop");
    lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelTricklerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void nnn_event_cb(lv_event_t *e)
{
  // Your code here
}

void nn_event_cb(lv_event_t *e)
{
  // Your code here
}

void n_event_cb(lv_event_t *e)
{
  // Your code here
}

void p_event_cb(lv_event_t *e)
{
  // Your code here
}

void add_event_cb(lv_event_t *e)
{
  // Your code here
}

void sub_event_cb(lv_event_t *e)
{
  // Your code here
}

void profile_event_cb(lv_event_t *e)
{
  lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
  char buffer[64];
  lv_roller_get_selected_str(ui_RollerProfile, buffer, sizeof(buffer));
  lv_label_set_text(ui_LabelMessages, buffer);
}

void message_event_cb(lv_event_t *e)
{
  lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
}

IRAM_ATTR void lvgl_disp_task(void *parg)
{
  while (1)
  {
    lv_timer_handler();
  }
}

IRAM_ATTR void disp_task_init(void)
{

  xTaskCreatePinnedToCore(lvgl_disp_task,  // task
                          "lvglTask",      // name for task
                          DISP_TASK_STACK, // size of task stack
                          NULL,            // parameters
                          DISP_TASK_PRO,   // priority
                          // nullptr,
                          &lv_disp_tcb,
                          DISP_TASK_CORE // must run the task on same core
                                         // core
  );
}

void setup()
{
  Serial.begin(115200); /* prepare for possible serial debug */
  displayInit();
  ui_init();
  disp_task_init();
  Serial.println("Setup done");
}

void loop()
{
  Serial.println("Loop");
  delay(5000);
}
