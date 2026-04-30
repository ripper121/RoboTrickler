#pragma once

#include "app_config.h"
#include "esp_err.h"

esp_err_t rt_storage_mount(void);
esp_err_t rt_storage_load_config(const char *path, rt_config_t *config);
esp_err_t rt_storage_load_profile(const char *profile_name, rt_config_t *config);
esp_err_t rt_storage_save_config(const char *path, const rt_config_t *config);
esp_err_t rt_storage_save_profile_target(const char *profile_name, float target_weight);
esp_err_t rt_storage_get_profile_list(char names[RT_MAX_PROFILE_NAMES][32], uint8_t *count);
esp_err_t rt_storage_create_profile_from_calibration(const rt_config_t *config, float weight, char *profile_name, size_t profile_name_size);
const char *rt_storage_mount_point(void);
const char *rt_storage_last_error(void);
