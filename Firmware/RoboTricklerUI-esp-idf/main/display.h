#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lv_conf.h"
#include "lvgl.h"

// ============================================================
// ST7796 (480x320) + XPT2046 touch display driver
// Replaces TFT_eSPI in the original Arduino project.
// ============================================================

// LV_HOR_RES_MAX and LV_VER_RES_MAX come from lv_conf.h (480 and 320)

#ifdef __cplusplus
extern "C" {
#endif

// Initialise display (ST7796) and touch (XPT2046), register LVGL drivers
void display_init(void);

// LVGL flush callback — registered internally
void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);

// LVGL touch read callback — registered internally
void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif
