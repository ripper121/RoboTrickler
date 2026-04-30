#pragma once

#include "app_config.h"
#include "esp_err.h"

esp_err_t rt_web_start(const rt_config_t *config);
bool rt_web_is_active(void);
void rt_web_poll_reconnect(const rt_config_t *config);
