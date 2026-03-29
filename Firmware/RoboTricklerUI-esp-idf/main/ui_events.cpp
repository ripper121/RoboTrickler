#include "config.h"
#include "arduino_compat.h"

#include <string>
#include "esp_log.h"
#include "ui.h"

static const char *TAG = "ui_events";

// ============================================================
// Forward declarations
// ============================================================
extern void startTrickler();
extern void stopTrickler();
extern void beep(const std::string &mode);
extern void setProfile(int num);
extern void messageBox(const std::string &msg, const lv_font_t *font, lv_color_t color, bool wait);

static int profileListCounter = 0;

// ============================================================
// Logger start/stop
// ============================================================
void startLogger() {
    strlcpy(config.mode, "logger", sizeof(config.mode));
    lv_label_set_text(ui_LabelLoggerStart, "Stop");
    lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    extern void startMeasurment();
    startMeasurment();
}

void stopLogger() {
    extern void stopMeasurment();
    stopMeasurment();
    lv_label_set_text(ui_LabelLoggerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonLoggerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelLoggerWeight, "-.-");
    lv_label_set_text(ui_LabelInfo, "");
    lv_label_set_text(ui_LabelLoggerInfo, "");
}

// ============================================================
// Button event callbacks (registered in ui.c / SquareLine generated)
// The SquareLine event names map to these via ui_events.h
// ============================================================

void trickler_start_event_cb(lv_event_t *e) {
    if (std::string(lv_label_get_text(ui_LabelTricklerStart)) == "Start") {
        ESP_LOGD(TAG, "TricklerStart");
        stopLogger();
        startTrickler();
    } else {
        ESP_LOGD(TAG, "TricklerStop");
        stopTrickler();
    }
}

void logger_start_event_cb(lv_event_t *e) {
    if (std::string(lv_label_get_text(ui_LabelLoggerStart)) == "Start") {
        ESP_LOGD(TAG, "LoggerStart");
        stopTrickler();
        startLogger();
    } else {
        ESP_LOGD(TAG, "LoggerStop");
        stopLogger();
    }
}

void nnn_event_cb(lv_event_t *e) { addWeight = 0.001f; beep("button"); }
void nn_event_cb(lv_event_t *e)  { addWeight = 0.01f;  beep("button"); }
void n_event_cb(lv_event_t *e)   { addWeight = 0.1f;   beep("button"); }
void p_event_cb(lv_event_t *e)   { addWeight = 1.0f;   beep("button"); }

void add_event_cb(lv_event_t *e) {
    config.targetWeight += addWeight;
    if (config.targetWeight > MAX_TARGET_WEIGHT) config.targetWeight = MAX_TARGET_WEIGHT;
    beep("button");
    lv_label_set_text(ui_LabelTarget, str_float(config.targetWeight, 3).c_str());
}

void sub_event_cb(lv_event_t *e) {
    config.targetWeight -= addWeight;
    if (config.targetWeight < 0) config.targetWeight = 0.0f;
    beep("button");
    lv_label_set_text(ui_LabelTarget, str_float(config.targetWeight, 3).c_str());
}

void prev_event_cb(lv_event_t *e) {
    profileListCounter--;
    if (profileListCounter < 0) profileListCounter = (int)(profileListCount - 1);
    setProfile(profileListCounter);
}

void next_event_cb(lv_event_t *e) {
    profileListCounter++;
    if (profileListCounter > (int)(profileListCount - 1)) profileListCounter = 0;
    setProfile(profileListCounter);
}

void message_event_cb(lv_event_t *e) {
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    if (restart_now) {
        delay(1000);
        esp_restart();
    }
}
