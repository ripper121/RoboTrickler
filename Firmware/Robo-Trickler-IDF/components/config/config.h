#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_PATH "/sdcard/config.json"

typedef struct {
    int  baud;
    char separator[8];
    char terminator[8];
    char request[32];
    int  request_delay_ms;
    char tare[32];
    bool tare_auto;
    int  tare_delay_ms;
    int  timeout_ms;
} config_scale_t;

typedef struct {
    bool ap;
    char ssid[64];
    char psk[64];
    char static_ip[32]; /* empty string when null in JSON */
    char gateway[32];
    char subnet[32];
    char dns[32];
} config_wifi_t;

typedef struct {
    bool check;
    char url[128];
} config_fw_update_t;

typedef enum {
    CONFIG_BUZZER_MODE_OFF = 0,
    CONFIG_BUZZER_MODE_DONE,
    CONFIG_BUZZER_MODE_BUTTON,
    CONFIG_BUZZER_MODE_BOTH,
} config_buzzer_mode_t;

typedef struct {
    config_scale_t     scale;
    config_wifi_t      wifi;
    config_fw_update_t fw_update;
    char               profile[32];
    char               language[8];
    config_buzzer_mode_t buzzer_mode;
    char               key[64];
    bool               debug;
} app_config_t;

/**
 * @brief Load and parse CONFIG_PATH into *cfg.
 *
 * Call once after the SD card is mounted. Pass the populated struct
 * (or a pointer to it) to every function that needs configuration.
 *
 * @param[out] cfg  Caller-allocated struct to populate.
 * @return
 *   - ESP_OK            on success
 *   - ESP_ERR_NOT_FOUND if the file does not exist
 *   - ESP_ERR_NO_MEM    if JSON parsing fails due to memory
 *   - ESP_FAIL          on any other parse error
 */
esp_err_t app_config_load(app_config_t *cfg);
esp_err_t app_config_save(const app_config_t *cfg);

#ifdef __cplusplus
}
#endif
