#pragma once

#include "app_config.h"
#include "esp_err.h"

typedef struct {
    float weight;
    int decimal_places;
    char unit[8];
    bool has_new_weight;
} rt_scale_reading_t;

esp_err_t rt_scale_init(int baud);
esp_err_t rt_scale_poll(const rt_config_t *config, rt_scale_reading_t *reading, uint32_t timeout_ms);
