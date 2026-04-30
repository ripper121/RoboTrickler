#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

esp_err_t rt_display_init(void);
bool rt_lvgl_lock(uint32_t timeout_ms);
void rt_lvgl_unlock(void);
void rt_ui_set_label(lv_obj_t *label, const char *text);
void rt_ui_set_label_color(lv_obj_t *label, uint32_t color);
void rt_ui_set_obj_bg(lv_obj_t *obj, uint32_t color);
void rt_ui_set_weight(float weight, int decimals, const char *unit);
void rt_ui_log(const char *text, bool no_history);
void rt_ui_message(const char *message, const lv_font_t *font, lv_color_t color, bool wait);
bool rt_ui_confirm(const char *message, const lv_font_t *font, lv_color_t color);
void rt_ui_apply_static_text(const char *start_text, const char *ok_text);
