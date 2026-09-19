#ifndef ROBOTRICKLER_UI_TOUCH_H
#define ROBOTRICKLER_UI_TOUCH_H

#include "lvgl.h"

// 480 x 320 display; approximately 8.6 mm at the configured 130 DPI.
// These are device pixels, not web CSS pixels or Apple points.
#define UI_TOUCH_TARGET_SIZE 44
#define UI_TOUCH_GAP 8
#define UI_ROW_PITCH (UI_TOUCH_TARGET_SIZE + UI_TOUCH_GAP)

#ifdef __cplusplus
extern "C" {
#endif

void prepareTouchLabel(lv_obj_t *label);
void prepareTouchButton(lv_obj_t *button);
void prepareTouchControls(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif
