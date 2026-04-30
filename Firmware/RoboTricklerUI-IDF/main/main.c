#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_api.h"
#include "app_config.h"
#include "board.h"
#include "cJSON.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota.h"
#include "scale.h"
#include "stepper.h"
#include "storage.h"
#include "ui.h"
#include "web_server.h"

static const char *TAG = "robo_trickler";

static rt_config_t s_config;
static rt_scale_reading_t s_scale;
static char s_profile_names[RT_MAX_PROFILE_NAMES][32];
static uint8_t s_profile_count;
static int s_profile_index;
static float s_last_weight;
static float s_add_weight = 0.1f;
static int s_weight_counter;
static int s_measurement_count;
static bool s_running;
static bool s_finished;
static bool s_first_throw = true;
static bool s_calibration_prompt_pending;
static int64_t s_calibration_prompt_time_us;
static int64_t s_last_scale_read_us;
static cJSON *s_lang_doc;

static const char *language_fallback(const char *key)
{
    if (strcmp(key, "tab_trickler") == 0) return "Trickler";
    if (strcmp(key, "tab_profile") == 0) return "Profile";
    if (strcmp(key, "tab_info") == 0) return "Info";
    if (strcmp(key, "button_start") == 0) return "Start";
    if (strcmp(key, "button_stop") == 0) return "Stop";
    if (strcmp(key, "button_ok") == 0) return "OK";
    if (strcmp(key, "button_yes") == 0) return "Yes";
    if (strcmp(key, "button_no") == 0) return "No";
    if (strcmp(key, "placeholder_profile") == 0) return "Profile";
    if (strcmp(key, "status_ready") == 0) return "Ready";
    if (strcmp(key, "status_stopped") == 0) return "Stopped";
    if (strcmp(key, "status_running") == 0) return "Running...";
    if (strcmp(key, "status_done") == 0) return "Done :)";
    if (strcmp(key, "status_timeout") == 0) return "Timeout! Check RS232 Wiring & Settings!";
    if (strcmp(key, "status_get_weight") == 0) return "Get Weight...";
    if (strcmp(key, "status_loading_profile") == 0) return "Loading profile: ";
    if (strcmp(key, "status_profile_ready") == 0) return "Profile ready: ";
    if (strcmp(key, "status_invalid_profile") == 0) return "Invalid profile!";
    if (strcmp(key, "status_invalid_stepper") == 0) return "Invalid stepper number!";
    if (strcmp(key, "status_over_trickle") == 0) return "!Over trickle!";
    if (strcmp(key, "msg_over_trickle") == 0) return "!Over trickle!\n!Check weight!";
    if (strcmp(key, "status_bulk_failed") == 0) return "Bulk trickle failed!";
    if (strcmp(key, "status_saving_target") == 0) return "Writing target to profile...";
    if (strcmp(key, "status_target_saved") == 0) return "Target saved";
    if (strcmp(key, "status_saving_target_failed") == 0) return "Target weight save failed!";
    if (strcmp(key, "status_init_steppers") == 0) return "Init steppers...";
    if (strcmp(key, "status_mount_sd") == 0) return "Mounting SD card...";
    if (strcmp(key, "status_loading_config") == 0) return "Loading config...";
    if (strcmp(key, "status_reading_profiles") == 0) return "Reading profiles...";
    if (strcmp(key, "status_starting_scale") == 0) return "Starting scale serial...";
    if (strcmp(key, "status_connect_wifi") == 0) return "Connect to Wifi: ";
    if (strcmp(key, "status_wifi_connected") == 0) return "Wifi Connected";
    if (strcmp(key, "status_no_wifi") == 0) return "No Wifi";
    if (strcmp(key, "status_open_browser_prefix") == 0) return "Open http://";
    if (strcmp(key, "status_open_browser_suffix") == 0) return ".local in your browser";
    if (strcmp(key, "msg_connect_wifi_wait") == 0) return "\n\nPlease wait...";
    if (strcmp(key, "msg_create_profile_prompt") == 0) return "Create profile from calibration?\n\nWeight: ";
    if (strcmp(key, "msg_profile_created") == 0) return "Profile created:\n\n";
    if (strcmp(key, "msg_create_profile_failed") == 0) return "Could not create profile!";
    return key;
}

static const char *lang_text(const char *key)
{
    if (s_lang_doc) {
        cJSON *ui = cJSON_GetObjectItemCaseSensitive(s_lang_doc, "ui");
        cJSON *item = cJSON_IsObject(ui) ? cJSON_GetObjectItemCaseSensitive(ui, key) : NULL;
        if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
            return item->valuestring;
        }
    }
    return language_fallback(key);
}

static char *read_small_text_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    if (len < 0 || len > 32 * 1024) {
        fclose(f);
        return NULL;
    }
    char *data = calloc(1, (size_t)len + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    if (fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return data;
}

static bool load_language(void)
{
    if (s_lang_doc) {
        cJSON_Delete(s_lang_doc);
        s_lang_doc = NULL;
    }
    for (char *p = s_config.language; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        } else if (*p == '-' || *p == '_') {
            *p = '\0';
            break;
        }
    }
    if (!s_config.language[0]) {
        strlcpy(s_config.language, "en", sizeof(s_config.language));
    }
    char candidates[4][64];
    snprintf(candidates[0], sizeof(candidates[0]), "%s/lang/%s.json", RT_SD_MOUNT_POINT, s_config.language);
    snprintf(candidates[1], sizeof(candidates[1]), "%s/system/lang/%s.json", RT_SD_MOUNT_POINT, s_config.language);
    snprintf(candidates[2], sizeof(candidates[2]), "%s/lang/en.json", RT_SD_MOUNT_POINT);
    snprintf(candidates[3], sizeof(candidates[3]), "%s/system/lang/en.json", RT_SD_MOUNT_POINT);
    for (size_t i = 0; i < 4; i++) {
        char *data = read_small_text_file(candidates[i]);
        if (!data) {
            continue;
        }
        cJSON *doc = cJSON_Parse(data);
        free(data);
        if (cJSON_IsObject(doc) && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(doc, "ui"))) {
            s_lang_doc = doc;
            ESP_LOGI(TAG, "Loaded language %s", candidates[i]);
            return true;
        }
        cJSON_Delete(doc);
    }
    ESP_LOGW(TAG, "Using built-in language fallback");
    return false;
}

static void apply_language(void)
{
    load_language();
    if (rt_lvgl_lock(pdMS_TO_TICKS(1000))) {
        lv_tabview_set_tab_text(ui_TabView, 0, lang_text("tab_trickler"));
        lv_tabview_set_tab_text(ui_TabView, 1, lang_text("tab_profile"));
        lv_tabview_set_tab_text(ui_TabView, 2, lang_text("tab_info"));
        if (!s_running) {
            lv_label_set_text(ui_LabelTricklerStart, lang_text("button_start"));
        }
        lv_label_set_text(ui_LabelButtonMessage, lang_text("button_ok"));
        if (strcmp(lv_label_get_text(ui_LabelProfile), "Profile") == 0) {
            lv_label_set_text(ui_LabelProfile, lang_text("placeholder_profile"));
        }
        rt_lvgl_unlock();
    }
}

static void update_log(const char *text, bool no_history)
{
    ESP_LOGI(TAG, "%s", text ? text : "");
    rt_ui_log(text, no_history);
}

static void beep(const char *mode)
{
    bool request_done = strstr(mode, "done") != NULL;
    bool request_button = strstr(mode, "button") != NULL;
    bool enable_done = strstr(s_config.beeper, "done") != NULL;
    bool enable_button = strstr(s_config.beeper, "button") != NULL;
    bool enable_both = strstr(s_config.beeper, "both") != NULL;
    if (request_done && (enable_done || enable_both)) {
        rt_stepper_beep(500);
    }
    if (request_button && (enable_button || enable_both)) {
        rt_stepper_beep(100);
    }
}

static long calculate_stepper_steps_for_units(double remaining_units, double units_per_throw, double *out_units)
{
    if (out_units) {
        *out_units = 0.0;
    }
    if (remaining_units <= 0.0 || units_per_throw <= 0.0) {
        return 0;
    }
    double exact_steps = remaining_units * (200.0 / units_per_throw);
    if (exact_steps <= 0.0 || exact_steps > 2147483647.0) {
        return 0;
    }
    long steps = (long)exact_steps;
    if (out_units) {
        *out_units = ((double)steps * units_per_throw) / 200.0;
    }
    return steps;
}

static bool run_bulk_stepper_move(char *info, size_t info_size)
{
    double remaining = (double)s_config.target_weight - (double)s_scale.weight - (double)s_config.profile_weight_gap;
    if (remaining <= 0.0) {
        return true;
    }
    for (int stepper = 2; stepper >= 1; stepper--) {
        if (!s_config.profile_stepper_enabled[stepper]) {
            continue;
        }
        int speed = s_config.profile_stepper_units_per_throw_speed[stepper] > 0 ? s_config.profile_stepper_units_per_throw_speed[stepper] : 200;
        double units = 0.0;
        long steps = calculate_stepper_steps_for_units(remaining, s_config.profile_stepper_units_per_throw[stepper], &units);
        if (steps <= 0) {
            continue;
        }
        rt_stepper_set_speed(stepper, speed);
        rt_stepper_step(stepper, steps, false);
        char chunk[32];
        snprintf(chunk, sizeof(chunk), "B%d ST%ld ", stepper, steps);
        strlcat(info, chunk, info_size);
        remaining -= units;
        if (remaining < 0.0) {
            remaining = 0.0;
        }
    }
    return true;
}

static void start_calibration_profile_prompt(void)
{
    rt_app_stop();
    s_calibration_prompt_pending = true;
    s_calibration_prompt_time_us = esp_timer_get_time();
    s_measurement_count = s_config.profile_general_measurements;
    s_weight_counter = 0;
}

static void handle_calibration_profile_prompt(void)
{
    if (!s_calibration_prompt_pending || s_last_scale_read_us <= s_calibration_prompt_time_us) {
        return;
    }
    s_calibration_prompt_pending = false;
    char prompt[96];
    snprintf(prompt, sizeof(prompt), "%s%.3f gn", lang_text("msg_create_profile_prompt"), s_scale.weight);
    if (rt_ui_confirm(prompt, &lv_font_montserrat_14, lv_color_hex(0xFFFFFF))) {
        char profile_name[32];
        if (rt_storage_create_profile_from_calibration(&s_config, s_scale.weight, profile_name, sizeof(profile_name)) == ESP_OK) {
            rt_storage_get_profile_list(s_profile_names, &s_profile_count);
            for (uint8_t i = 0; i < s_profile_count; i++) {
                if (strcmp(s_profile_names[i], profile_name) == 0) {
                    rt_app_set_profile_index(i);
                    break;
                }
            }
            char msg[80];
            snprintf(msg, sizeof(msg), "%s%s.txt", lang_text("msg_profile_created"), profile_name);
            rt_ui_message(msg, &lv_font_montserrat_14, lv_color_hex(0x00FF00), true);
        } else {
            rt_ui_message(lang_text("msg_create_profile_failed"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        }
    }
}

static bool scale_weight_stable(void)
{
    float tolerance = 0.5f;
    for (int i = 0; i < s_scale.decimal_places && i < 6; i++) {
        tolerance *= 0.1f;
    }
    if (fabsf(s_scale.weight - s_last_weight) <= tolerance) {
        if (s_running || s_calibration_prompt_pending) {
            s_weight_counter++;
        }
    } else {
        s_weight_counter = 0;
        rt_ui_set_weight(s_scale.weight, s_scale.decimal_places, s_scale.unit);
    }
    s_last_weight = s_scale.weight;
    int required = s_measurement_count > 0 ? s_measurement_count : s_config.profile_general_measurements;
    return s_weight_counter >= required;
}

static void run_profile_step(void)
{
    char info[96] = {0};
    if (s_first_throw) {
        s_first_throw = false;
        if (!run_bulk_stepper_move(info, sizeof(info))) {
            update_log(lang_text("status_bulk_failed"), true);
            rt_app_stop();
            return;
        }
        if (info[0]) {
            if (strcmp(s_config.profile, "calibrate") == 0) {
                start_calibration_profile_prompt();
                return;
            }
            s_measurement_count = s_config.profile_general_measurements;
            update_log(info, true);
            return;
        }
    }
    if (s_config.profile_count <= 0) {
        update_log(lang_text("status_invalid_profile"), true);
        rt_app_stop();
        return;
    }
    int profile_step = s_config.profile_count - 1;
    for (int i = 0; i < s_config.profile_count; i++) {
        if (s_scale.weight <= (s_config.target_weight - s_config.profile_weight[i])) {
            profile_step = i;
            break;
        }
    }
    int stepper = s_config.profile_num[profile_step];
    if (stepper < 1 || stepper > 2) {
        update_log(lang_text("status_invalid_stepper"), true);
        rt_app_stop();
        return;
    }
    rt_stepper_set_speed(stepper, s_config.profile_speed[profile_step]);
    rt_stepper_step(stepper, s_config.profile_steps[profile_step], s_config.profile_reverse[profile_step]);
    s_measurement_count = s_config.profile_measurements[profile_step];
    s_weight_counter = 0;
    snprintf(info, sizeof(info), "W%.3f ST%ld SP%d M%d",
             s_config.profile_weight[profile_step],
             s_config.profile_steps[profile_step],
             s_config.profile_speed[profile_step],
             s_config.profile_measurements[profile_step]);
    if (strcmp(s_config.profile, "calibrate") == 0) {
        start_calibration_profile_prompt();
        return;
    }
    update_log(info, true);
}

void rt_app_start(void)
{
    char info[96];
    snprintf(info, sizeof(info), "%s%s", lang_text("status_loading_profile"), s_config.profile);
    update_log(info, true);
    if (rt_storage_load_profile(s_config.profile, &s_config) != ESP_OK) {
        update_log(rt_storage_last_error(), false);
        rt_ui_message(rt_storage_last_error(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        return;
    }
    rt_ui_set_label(ui_LabelTricklerStart, lang_text("button_stop"));
    rt_ui_set_obj_bg(ui_ButtonTricklerStart, 0xFF0000);
    s_running = true;
    s_finished = false;
    s_first_throw = true;
    s_weight_counter = 0;
    s_measurement_count = s_config.profile_general_measurements;
    s_last_weight = s_scale.weight;
    beep("button");
    update_log(lang_text("status_running"), true);
}

void rt_app_stop(void)
{
    s_running = false;
    s_finished = true;
    rt_ui_set_label(ui_LabelTricklerStart, lang_text("button_start"));
    rt_ui_set_obj_bg(ui_ButtonTricklerStart, 0x00FF00);
    rt_ui_set_label(ui_LabelTricklerWeight, "-.-");
    beep("button");
    update_log(lang_text("status_stopped"), true);
}

void rt_app_set_profile_index(int index)
{
    if (index < 0 || index >= s_profile_count) {
        return;
    }
    strlcpy(s_config.profile, s_profile_names[index], sizeof(s_config.profile));
    s_profile_index = index;
    rt_storage_save_config("/config.txt", &s_config);
    if (rt_storage_load_profile(s_config.profile, &s_config) != ESP_OK) {
        update_log(rt_storage_last_error(), false);
        return;
    }
    char text[32];
    snprintf(text, sizeof(text), "%.3f", s_config.target_weight);
    rt_ui_set_label(ui_LabelProfile, s_config.profile);
    rt_ui_set_label(ui_LabelTarget, text);
    char info[80];
    snprintf(info, sizeof(info), "%s%s", lang_text("status_profile_ready"), s_config.profile);
    update_log(info, true);
}

void rt_app_set_target(float target)
{
    s_config.target_weight = target;
    char text[32];
    snprintf(text, sizeof(text), "%.3f", s_config.target_weight);
    rt_ui_set_label(ui_LabelTarget, text);
    update_log(lang_text("status_saving_target"), true);
    if (rt_storage_save_profile_target(s_config.profile, target) == ESP_OK) {
        update_log(lang_text("status_target_saved"), true);
    } else {
        update_log(rt_storage_last_error(), true);
    }
}

float rt_app_get_weight(void) { return s_scale.weight; }
float rt_app_get_target(void) { return s_config.target_weight; }
const char *rt_app_get_profile(void) { return s_config.profile; }
const char *rt_app_get_language(void) { return s_config.language; }
bool rt_app_is_running(void) { return s_running; }

static void log_heap_stats(void)
{
    ESP_LOGI(TAG,
             "Heap: Free:%u, Min:%u, Size:%u, Alloc:%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

void rt_app_get_profile_list(char names[RT_MAX_PROFILE_NAMES][32], uint8_t *count)
{
    for (uint8_t i = 0; i < s_profile_count; i++) {
        strlcpy(names[i], s_profile_names[i], 32);
    }
    *count = s_profile_count;
}

void trickler_start_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_running) {
        rt_app_stop();
    } else {
        rt_app_start();
    }
}

void nnn_event_cb(lv_event_t *e) { (void)e; s_add_weight = 0.001f; beep("button"); }
void nn_event_cb(lv_event_t *e) { (void)e; s_add_weight = 0.01f; beep("button"); }
void n_event_cb(lv_event_t *e) { (void)e; s_add_weight = 0.1f; beep("button"); }
void p_event_cb(lv_event_t *e) { (void)e; s_add_weight = 1.0f; beep("button"); }

void add_event_cb(lv_event_t *e)
{
    (void)e;
    s_config.target_weight += s_add_weight;
    if (s_config.target_weight > RT_MAX_TARGET_WEIGHT) {
        s_config.target_weight = RT_MAX_TARGET_WEIGHT;
    }
    char text[32];
    snprintf(text, sizeof(text), "%.3f", s_config.target_weight);
    rt_ui_set_label(ui_LabelTarget, text);
    beep("button");
}

void sub_event_cb(lv_event_t *e)
{
    (void)e;
    s_config.target_weight -= s_add_weight;
    if (s_config.target_weight < 0.0f) {
        s_config.target_weight = 0.0f;
    }
    char text[32];
    snprintf(text, sizeof(text), "%.3f", s_config.target_weight);
    rt_ui_set_label(ui_LabelTarget, text);
    beep("button");
}

void prev_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_profile_count == 0) {
        return;
    }
    s_profile_index = s_profile_index <= 0 ? s_profile_count - 1 : s_profile_index - 1;
    rt_app_set_profile_index(s_profile_index);
}

void next_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_profile_count == 0) {
        return;
    }
    s_profile_index = (s_profile_index + 1) % s_profile_count;
    rt_app_set_profile_index(s_profile_index);
}

static void control_task(void *arg)
{
    (void)arg;
    int64_t last_idle_read_us = 0;
    while (true) {
        esp_err_t ret = rt_scale_poll(&s_config, &s_scale, s_running ? 2000 : 50);
        if (ret == ESP_OK && s_scale.has_new_weight) {
            s_last_scale_read_us = esp_timer_get_time();
            rt_ui_set_weight(s_scale.weight, s_scale.decimal_places, s_scale.unit);
            if (s_running) {
                if (scale_weight_stable()) {
                    s_weight_counter = 0;
                    if (s_scale.weight >= (s_config.target_weight - s_config.profile_tolerance)) {
                        if (s_scale.weight <= (s_config.target_weight + s_config.profile_tolerance)) {
                            rt_ui_set_label_color(ui_LabelTricklerWeight, 0x00FF00);
                        } else {
                            rt_ui_set_label_color(ui_LabelTricklerWeight, 0xFFFF00);
                        }
                        if (s_scale.weight >= (s_config.target_weight + s_config.profile_alarm_threshold) && s_config.profile_alarm_threshold > 0.0f) {
                            rt_ui_set_label_color(ui_LabelTricklerWeight, 0xFF0000);
                            update_log(lang_text("status_over_trickle"), true);
                            beep("done");
                            beep("done");
                            beep("done");
                            rt_app_stop();
                            rt_ui_message(lang_text("msg_over_trickle"), &lv_font_montserrat_32, lv_color_hex(0xFF0000), true);
                        }
                        if (!s_finished) {
                            beep("done");
                            update_log(lang_text("status_done"), true);
                        }
                        s_finished = true;
                    } else {
                        rt_ui_set_label_color(ui_LabelTricklerWeight, 0xFFFFFF);
                        run_profile_step();
                    }
                }
            } else {
                handle_calibration_profile_prompt();
            }
        } else if (ret == ESP_ERR_TIMEOUT && s_running) {
            update_log(lang_text("status_timeout"), true);
        }
        int64_t now = esp_timer_get_time();
        if (!s_running && now - last_idle_read_us > 1000000LL) {
            last_idle_read_us = now;
            update_log(lang_text("status_get_weight"), true);
        }
        rt_web_poll_reconnect(&s_config);
        vTaskDelay(pdMS_TO_TICKS(s_running ? 10 : 250));
    }
}

void app_main(void)
{
    rt_config_set_defaults(&s_config);
    ESP_LOGI(TAG, "Robo-Trickler %s starting", RT_FW_VERSION);
    ESP_ERROR_CHECK(rt_display_init());
    update_log("Robo-Trickler v2.10 // strenuous.dev", false);
    ESP_ERROR_CHECK(rt_stepper_init());
    update_log("Mounting SD card...", true);
    esp_err_t ret = rt_storage_mount();
    if (ret != ESP_OK) {
        update_log(rt_storage_last_error(), false);
        rt_ui_message(rt_storage_last_error(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
    } else {
        if (rt_storage_load_config("/config.txt", &s_config) != ESP_OK) {
            update_log(rt_storage_last_error(), false);
            rt_storage_save_config("/config.txt", &s_config);
        }
        rt_storage_get_profile_list(s_profile_names, &s_profile_count);
        for (uint8_t i = 0; i < s_profile_count; i++) {
            if (strcmp(s_profile_names[i], s_config.profile) == 0) {
                s_profile_index = i;
                break;
            }
        }
        if (rt_storage_load_profile(s_config.profile, &s_config) != ESP_OK) {
            update_log(rt_storage_last_error(), false);
        }
        rt_ota_from_sd();
    }
    ESP_ERROR_CHECK(rt_scale_init(s_config.scale_baud));
    char text[32];
    snprintf(text, sizeof(text), "%.3f", s_config.target_weight);
    rt_ui_set_label(ui_LabelTarget, text);
    rt_ui_set_label(ui_LabelProfile, s_config.profile);
    apply_language();
    if (s_config.wifi_ssid[0]) {
        if (rt_web_start(&s_config) == ESP_OK) {
            update_log("Open http://robo-trickler.local in your browser", false);
        } else {
            update_log("No Wifi", false);
        }
    }
    update_log(lang_text("status_ready"), true);
    log_heap_stats();
    BaseType_t task_ret = xTaskCreatePinnedToCore(control_task,
                                                  "trickler",
                                                  RT_APP_TASK_STACK_BYTES,
                                                  NULL,
                                                  RT_APP_TASK_PRIORITY,
                                                  NULL,
                                                  RT_APP_TASK_CORE);
    ESP_ERROR_CHECK(task_ret == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
