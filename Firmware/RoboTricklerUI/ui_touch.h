#ifndef ROBOTRICKLER_UI_TOUCH_H
#define ROBOTRICKLER_UI_TOUCH_H

#include "lvgl.h"

// 480 x 320 display; approximately 9.8 mm at the configured 130 DPI.
// These are device pixels, not web CSS pixels or Apple points.
#define UI_TOUCH_TARGET_SIZE 50
#define UI_TOUCH_GAP 8

#ifdef __cplusplus
extern "C" {
#endif

void prepareTouchButton(lv_obj_t *button);
void prepareTouchControls(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif
