#pragma once

#include "esp_err.h"

esp_err_t rt_stepper_init(void);
void rt_stepper_set_speed(int stepper_num, int rpm);
esp_err_t rt_stepper_step(int stepper_num, long steps, bool reverse);
esp_err_t rt_stepper_beep(unsigned long time_ms);
