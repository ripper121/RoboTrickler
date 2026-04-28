#include "ui.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "actuator.h"
#include "display.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "generated/ui_generated.h"
#include "profile.h"
#include "scale.h"
#include "trickle.h"

static const char *TAG = "ui";

#define UI_MAX_PROFILES          32U
#define UI_TARGET_MIN            0.0
#define UI_TARGET_MAX            999.0
#define UI_LOCK_TIMEOUT_MS       1000U
#define UI_SCALE_TASK_STACK_SIZE 4096U
#define UI_SCALE_TASK_PRIORITY   3U
#define UI_SCALE_POLL_MS         1000U
#define UI_INFO_LINES            12U
#define UI_INFO_LINE_LEN         64U

typedef struct {
    app_config_t config;
    app_profile_t profile;
    actuator_handle_t actuator;
    scale_handle_t scale;
    ui_service_control_fn_t webserver_start;
    ui_service_control_fn_t webserver_stop;
    char profile_names[UI_MAX_PROFILES][PROFILE_NAME_MAX_LEN];
    size_t profile_count;
    size_t active_profile_index;
    double target_weight;
    double increment;
    bool initialized;
    bool last_trickle_running;
    bool wifi_connected;
    char info_lines[UI_INFO_LINES][UI_INFO_LINE_LEN];
} ui_state_t;

static ui_state_t s_ui;

static void ui_set_label(lv_obj_t *label, const char *text)
{
    if (label == NULL || text == NULL) {
        return;
    }

    if (display_lock(UI_LOCK_TIMEOUT_MS)) {
        lv_label_set_text(label, text);
        display_unlock();
    }
}

static void ui_set_button_state(lv_obj_t *button, lv_obj_t *label, bool running)
{
    if (display_lock(UI_LOCK_TIMEOUT_MS)) {
        lv_label_set_text(label, running ? "Stop" : "Start");
        lv_obj_set_style_bg_color(button,
                                  lv_color_hex(running ? 0xFF0000 : 0x00AA40),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        display_unlock();
    }
}

static void ui_format_target(void)
{
    char text[24];

    snprintf(text, sizeof(text), "%.3f", s_ui.target_weight);
    ui_set_label(ui_LabelTarget, text);
}

static void ui_format_weight(float value)
{
    char text[24];

    snprintf(text, sizeof(text), "%.3f g", (double)value);
    ui_set_label(ui_LabelTricklerWeight, text);
}

static void ui_push_info(const char *text)
{
    char log_text[(UI_INFO_LINES * UI_INFO_LINE_LEN) + UI_INFO_LINES];

    if (text == NULL) {
        return;
    }

    memmove(&s_ui.info_lines[0],
            &s_ui.info_lines[1],
            (UI_INFO_LINES - 1U) * sizeof(s_ui.info_lines[0]));
    snprintf(s_ui.info_lines[UI_INFO_LINES - 1U], sizeof(s_ui.info_lines[0]), "%s", text);

    log_text[0] = '\0';
    for (size_t i = 0; i < UI_INFO_LINES; i++) {
        if (s_ui.info_lines[i][0] == '\0') {
            continue;
        }
        strlcat(log_text, s_ui.info_lines[i], sizeof(log_text));
        strlcat(log_text, "\n", sizeof(log_text));
    }

    ui_set_label(ui_LabelInfo, text);
    ui_set_label(ui_LabelLog, log_text);
}

static void ui_show_message(const char *message, uint32_t color)
{
    if (message == NULL) {
        return;
    }

    if (display_lock(UI_LOCK_TIMEOUT_MS)) {
        lv_obj_set_style_text_color(ui_LabelMessages, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_LabelMessages, message);
        lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
        display_unlock();
    }
}

static void ui_update_profile_label(void)
{
    ui_set_label(ui_LabelProfile, s_ui.profile.name[0] != '\0' ? s_ui.profile.name : "No profile");
}

static void ui_update_trickle_buttons(bool running)
{
    ui_set_button_state(ui_ButtonTricklerStart, ui_LabelTricklerStart, running);
}

static bool ui_done_beep_enabled(void)
{
    return s_ui.config.buzzer_mode == CONFIG_BUZZER_MODE_DONE ||
           s_ui.config.buzzer_mode == CONFIG_BUZZER_MODE_BOTH;
}

static bool ui_button_beep_enabled(void)
{
    return s_ui.config.buzzer_mode == CONFIG_BUZZER_MODE_BUTTON ||
           s_ui.config.buzzer_mode == CONFIG_BUZZER_MODE_BOTH;
}

static void ui_beep_button(void)
{
    if (s_ui.actuator != NULL && ui_button_beep_enabled()) {
        (void)actuator_beep_ms(s_ui.actuator, 100U);
    }
}

static bool ui_trickle_should_stop(void *ctx)
{
    (void)ctx;
    return false;
}

static void ui_trickle_progress(double applied_units, void *ctx)
{
    char text[48];

    (void)ctx;
    snprintf(text, sizeof(text), "Applied %.3f units", applied_units);
    ui_push_info(text);
}

static void ui_trickle_warning(trickle_warning_t warning, void *ctx)
{
    (void)ctx;

    switch (warning) {
    case TRICKLE_WARNING_RS232_TIMEOUT:
        ui_push_info("RS232 timeout");
        break;
    case TRICKLE_WARNING_RS232_ALARM:
        ui_show_message("Over trickle\nCheck weight", 0xFF0000);
        break;
    case TRICKLE_WARNING_RS232_READ_FAIL:
    default:
        ui_push_info("RS232 read fail");
        break;
    }
}

static void ui_scale_task(void *arg)
{
    (void)arg;

    while (true) {
        bool running = false;

        if (s_ui.initialized && s_ui.scale != NULL && trickle_is_running() == false) {
            float value = 0.0f;
            esp_err_t ret = scale_read_unit(s_ui.scale, &value);
            if (ret == ESP_OK) {
                ui_format_weight(value);
            }
        }

        (void)ui_get_trickle_running(&running);
        vTaskDelay(pdMS_TO_TICKS(running ? 250U : UI_SCALE_POLL_MS));
    }
}

static esp_err_t ui_ensure_profile_dir(void)
{
    struct stat st = {0};

    if (stat(PROFILE_DIR, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_FAIL;
    }

    if (mkdir(PROFILE_DIR, 0775) != 0) {
        ESP_LOGW(TAG, "mkdir failed for %s", PROFILE_DIR);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t ui_create_default_profile(void)
{
    app_profile_t profile = {0};

    ESP_RETURN_ON_ERROR(ui_ensure_profile_dir(), TAG, "profile dir create failed");
    ESP_RETURN_ON_ERROR(profile_init_default(&profile, "powder"),
                        TAG, "default profile init failed");
    return profile_save(&profile);
}

esp_err_t ui_reload_profiles(void)
{
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    ESP_RETURN_ON_ERROR(ui_ensure_profile_dir(), TAG, "profile dir unavailable");

    s_ui.profile_count = 0;
    dir = opendir(PROFILE_DIR);
    if (dir == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    while ((entry = readdir(dir)) != NULL && s_ui.profile_count < UI_MAX_PROFILES) {
        const char *dot = strrchr(entry->d_name, '.');
        size_t name_len = 0;

        if (dot == NULL || strcmp(dot, ".json") != 0) {
            continue;
        }

        name_len = (size_t)(dot - entry->d_name);
        if (name_len == 0U || name_len >= PROFILE_NAME_MAX_LEN) {
            continue;
        }

        memcpy(s_ui.profile_names[s_ui.profile_count], entry->d_name, name_len);
        s_ui.profile_names[s_ui.profile_count][name_len] = '\0';
        s_ui.profile_count++;
    }

    closedir(dir);

    if (s_ui.profile_count == 0U) {
        ESP_RETURN_ON_ERROR(ui_create_default_profile(), TAG, "default profile create failed");
        return ui_reload_profiles();
    }

    return ESP_OK;
}

static esp_err_t ui_load_active_profile(const char *preferred_name)
{
    const char *profile_name = preferred_name;
    esp_err_t ret = ESP_OK;

    if (s_ui.profile_count == 0U) {
        ESP_RETURN_ON_ERROR(ui_reload_profiles(), TAG, "profile reload failed");
    }

    if (profile_name == NULL || profile_name[0] == '\0') {
        profile_name = s_ui.profile_names[0];
    }

    ret = profile_load(profile_name, &s_ui.profile);
    if (ret != ESP_OK && s_ui.profile_count > 0U) {
        ESP_LOGW(TAG, "Failed to load profile '%s', falling back to %s", profile_name, s_ui.profile_names[0]);
        profile_name = s_ui.profile_names[0];
        ret = profile_load(profile_name, &s_ui.profile);
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "profile load failed");

    for (size_t i = 0; i < s_ui.profile_count; i++) {
        if (strcmp(s_ui.profile_names[i], s_ui.profile.name) == 0) {
            s_ui.active_profile_index = i;
            break;
        }
    }

    s_ui.target_weight = s_ui.profile.general.target_weight;
    if (s_ui.target_weight <= 0.0) {
        s_ui.target_weight = 40.0;
    }
    return ESP_OK;
}

size_t ui_get_profile_count(void)
{
    return s_ui.profile_count;
}

const char *ui_get_profile_name_by_index(size_t index)
{
    return index < s_ui.profile_count ? s_ui.profile_names[index] : "";
}

esp_err_t ui_get_active_profile_name(char *out_name, size_t out_name_size)
{
    ESP_RETURN_ON_FALSE(out_name != NULL && out_name_size > 0U, ESP_ERR_INVALID_ARG, TAG, "output invalid");
    snprintf(out_name, out_name_size, "%s", s_ui.profile.name);
    return ESP_OK;
}

esp_err_t ui_select_profile(const char *profile_name)
{
    ESP_RETURN_ON_FALSE(profile_name != NULL && profile_name[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "profile name invalid");
    ESP_RETURN_ON_FALSE(!trickle_is_running(), ESP_ERR_INVALID_STATE, TAG, "trickle running");

    ESP_RETURN_ON_ERROR(ui_load_active_profile(profile_name), TAG, "load profile failed");
    snprintf(s_ui.config.profile, sizeof(s_ui.config.profile), "%s", s_ui.profile.name);
    (void)app_config_save(&s_ui.config);
    ui_update_profile_label();
    ui_format_target();
    ui_push_info("Profile selected");
    return ESP_OK;
}

esp_err_t ui_get_trickle_running(bool *out_running)
{
    ESP_RETURN_ON_FALSE(out_running != NULL, ESP_ERR_INVALID_ARG, TAG, "out_running is null");
    *out_running = trickle_is_running();
    return ESP_OK;
}

esp_err_t ui_start_trickle(void)
{
    trickle_execute_config_t config = {0};
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(s_ui.initialized, ESP_ERR_INVALID_STATE, TAG, "ui not initialized");
    ESP_RETURN_ON_FALSE(!trickle_is_running(), ESP_ERR_INVALID_STATE, TAG, "trickle already running");
    ESP_RETURN_ON_FALSE(s_ui.actuator != NULL, ESP_ERR_INVALID_STATE, TAG, "actuator not initialized");
    ESP_RETURN_ON_FALSE(s_ui.target_weight > 0.0 && isfinite(s_ui.target_weight),
                        ESP_ERR_INVALID_ARG, TAG, "target weight invalid");

    s_ui.profile.general.target_weight = s_ui.target_weight;
    (void)profile_save(&s_ui.profile);

    config.actuator = s_ui.actuator;
    config.profile = &s_ui.profile;
    config.scale = s_ui.scale;
    config.should_stop = ui_trickle_should_stop;
    config.progress = ui_trickle_progress;
    config.warning = ui_trickle_warning;
    config.done_beep_enabled = ui_done_beep_enabled();
    config.tare_auto = s_ui.config.scale.tare_auto;
    config.tare_delay_ms = s_ui.config.scale.tare_delay_ms;

    ret = trickle_execute(&config, s_ui.target_weight, s_ui.profile.general.trickle_gap);
    ESP_RETURN_ON_ERROR(ret, TAG, "trickle_execute failed");

    ui_update_trickle_buttons(true);
    ui_push_info("Running...");
    return ESP_OK;
}

esp_err_t ui_stop_trickle_request(void)
{
    if (!trickle_is_running()) {
        ui_update_trickle_buttons(false);
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(trickle_stop(), TAG, "trickle stop failed");
    ESP_RETURN_ON_ERROR(actuator_stop_all(s_ui.actuator), TAG, "actuator stop failed");
    ui_push_info("Stopping...");
    return ESP_OK;
}

esp_err_t ui_tare_scale(void)
{
    ESP_RETURN_ON_FALSE(s_ui.scale != NULL, ESP_ERR_INVALID_STATE, TAG, "scale not initialized");
    return scale_tare(s_ui.scale);
}

esp_err_t ui_get_target_weight(double *out_weight)
{
    ESP_RETURN_ON_FALSE(out_weight != NULL, ESP_ERR_INVALID_ARG, TAG, "out_weight is null");
    *out_weight = s_ui.target_weight;
    return ESP_OK;
}

esp_err_t ui_set_target_weight(double target_weight)
{
    ESP_RETURN_ON_FALSE(isfinite(target_weight), ESP_ERR_INVALID_ARG, TAG, "target is not finite");

    if (target_weight < UI_TARGET_MIN) {
        target_weight = UI_TARGET_MIN;
    }
    if (target_weight > UI_TARGET_MAX) {
        target_weight = UI_TARGET_MAX;
    }

    s_ui.target_weight = target_weight;
    s_ui.profile.general.target_weight = target_weight;
    (void)profile_save(&s_ui.profile);
    ui_format_target();
    return ESP_OK;
}

esp_err_t ui_draw(void)
{
    bool running = trickle_is_running();

    ui_format_target();
    ui_update_profile_label();
    ui_update_trickle_buttons(running);
    return ESP_OK;
}

esp_err_t ui_poll(void)
{
    bool running = trickle_is_running();

    if (running != s_ui.last_trickle_running) {
        s_ui.last_trickle_running = running;
        ui_update_trickle_buttons(running);
        if (!running) {
            trickle_result_t result = {0};
            esp_err_t status = ESP_OK;
            bool stopped = false;
            char text[64];

            (void)trickle_get_last_result(&result, &status, &stopped);
            snprintf(text,
                     sizeof(text),
                     stopped ? "Stopped" : (status == ESP_OK ? "Done" : "Trickle error"));
            ui_push_info(text);
        }
    }

    return ESP_OK;
}

esp_err_t ui_set_wifi_connected(bool connected)
{
    s_ui.wifi_connected = connected;
    ui_push_info(connected ? "WiFi connected" : "WiFi disconnected");
    return ESP_OK;
}

esp_err_t ui_set_webserver_callbacks(ui_service_control_fn_t start_cb,
                                     ui_service_control_fn_t stop_cb)
{
    s_ui.webserver_start = start_cb;
    s_ui.webserver_stop = stop_cb;
    return ESP_OK;
}

esp_err_t ui_init(const app_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(!s_ui.initialized, ESP_ERR_INVALID_STATE, TAG, "ui already initialized");

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.config = *config;
    s_ui.increment = 0.1;

    if (display_lock(UI_LOCK_TIMEOUT_MS)) {
        ui_generated_init();
        lv_obj_t *tab_content = lv_tabview_get_content(ui_TabView);
        lv_obj_clear_flag(tab_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(tab_content, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(tab_content, LV_SCROLLBAR_MODE_OFF);
        display_unlock();
    }

    ESP_RETURN_ON_ERROR(actuator_init(&s_ui.actuator), TAG, "actuator init failed");

    if (s_ui.config.scale.baud > 0 && s_ui.config.scale.terminator[0] != '\0') {
        esp_err_t ret = scale_init(&s_ui.config.scale, &s_ui.scale);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "scale init failed: %s", esp_err_to_name(ret));
            ui_push_info("Scale unavailable");
        }
    }

    ESP_RETURN_ON_ERROR(ui_reload_profiles(), TAG, "profile reload failed");
    ESP_RETURN_ON_ERROR(ui_load_active_profile(s_ui.config.profile), TAG, "active profile load failed");

    s_ui.initialized = true;
    ESP_RETURN_ON_FALSE(xTaskCreate(ui_scale_task,
                                    "ui_scale",
                                    UI_SCALE_TASK_STACK_SIZE,
                                    NULL,
                                    UI_SCALE_TASK_PRIORITY,
                                    NULL) == pdPASS,
                        ESP_ERR_NO_MEM,
                        TAG,
                        "scale task create failed");

    ui_draw();
    ui_push_info("Robo-Trickler ready");
    return ESP_OK;
}

void trickler_start_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    ui_beep_button();
    if (trickle_is_running()) {
        (void)ui_stop_trickle_request();
    } else {
        esp_err_t ret = ui_start_trickle();
        if (ret != ESP_OK) {
            ui_show_message("Trickle start failed", 0xFF0000);
        }
    }
}

void nnn_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    s_ui.increment = 0.001;
    ui_beep_button();
}

void nn_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    s_ui.increment = 0.01;
    ui_beep_button();
}

void n_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    s_ui.increment = 0.1;
    ui_beep_button();
}

void p_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    s_ui.increment = 1.0;
    ui_beep_button();
}

void add_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    ui_beep_button();
    (void)ui_set_target_weight(s_ui.target_weight + s_ui.increment);
}

void sub_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    ui_beep_button();
    (void)ui_set_target_weight(s_ui.target_weight - s_ui.increment);
}

void prev_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    ui_beep_button();

    if (s_ui.profile_count == 0U) {
        return;
    }

    s_ui.active_profile_index = (s_ui.active_profile_index == 0U) ?
                                (s_ui.profile_count - 1U) :
                                (s_ui.active_profile_index - 1U);
    (void)ui_select_profile(s_ui.profile_names[s_ui.active_profile_index]);
}

void next_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    ui_beep_button();

    if (s_ui.profile_count == 0U) {
        return;
    }

    s_ui.active_profile_index = (s_ui.active_profile_index + 1U) % s_ui.profile_count;
    (void)ui_select_profile(s_ui.profile_names[s_ui.active_profile_index]);
}

void message_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_ui.initialized) {
        return;
    }

    if (display_lock(UI_LOCK_TIMEOUT_MS)) {
        lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
        display_unlock();
    }
}
