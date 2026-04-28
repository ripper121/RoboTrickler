#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*ui_service_control_fn_t)(void);

esp_err_t ui_init(const app_config_t *config);
esp_err_t ui_draw(void);
esp_err_t ui_poll(void);
esp_err_t ui_set_wifi_connected(bool connected);
esp_err_t ui_set_webserver_callbacks(ui_service_control_fn_t start_cb,
                                     ui_service_control_fn_t stop_cb);
esp_err_t ui_reload_profiles(void);
size_t ui_get_profile_count(void);
const char *ui_get_profile_name_by_index(size_t index);
esp_err_t ui_get_active_profile_name(char *out_name, size_t out_name_size);
esp_err_t ui_select_profile(const char *profile_name);
esp_err_t ui_get_trickle_running(bool *out_running);
esp_err_t ui_start_trickle(void);
esp_err_t ui_stop_trickle_request(void);
esp_err_t ui_tare_scale(void);
esp_err_t ui_get_target_weight(double *out_weight);
esp_err_t ui_set_target_weight(double target_weight);

#ifdef __cplusplus
}
#endif
