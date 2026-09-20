#ifndef ROBOTRICKLER_UI_TOUCH_H
#define ROBOTRICKLER_UI_TOUCH_H

#include "lvgl.h"

// 480 x 320 display; approximately 8.6 mm at the configured 130 DPI.
// These are device pixels, not web CSS pixels or Apple points.
#define UI_TOUCH_TARGET_SIZE 44
#define UI_TOUCH_GAP 8
#define UI_ROW_PITCH (UI_TOUCH_TARGET_SIZE + UI_TOUCH_GAP)
#define UI_PROFILE_NAV_HEIGHT (2 * UI_TOUCH_TARGET_SIZE)
#define UI_PROFILE_CENTER_HEIGHT 75
#define UI_PROFILE_ACTION_PADDING 5
#define UI_PROFILE_ACTION_SIZE (UI_PROFILE_CENTER_HEIGHT - (2 * UI_PROFILE_ACTION_PADDING))
#define UI_DIALOG_CONTENT_WIDTH 404
#define UI_DIALOG_SIDE_BUTTON_WIDTH 56
#define UI_DIALOG_SIDE_BUTTON_X 174
#define UI_DIALOG_VALUE_WIDTH 276
#define UI_DIALOG_ENTRY_STEP_WIDTH 340
#define UI_DIALOG_ENTRY_STEP_X -32
#define UI_DIALOG_ACTION_WIDTH 198
#define UI_DIALOG_ACTION_X 103
#define UI_DIALOG_SELECTOR_Y -114
#define UI_DIALOG_VALUE_Y -57
#define UI_DIALOG_ENTRY_Y 0
#define UI_DIALOG_TEST_Y 57
#define UI_DIALOG_ACTION_Y 114

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
