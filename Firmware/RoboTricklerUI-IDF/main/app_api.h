#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

void rt_app_start(void);
void rt_app_stop(void);
void rt_app_set_profile_index(int index);
void rt_app_set_target(float target);
float rt_app_get_weight(void);
float rt_app_get_target(void);
const char *rt_app_get_profile(void);
const char *rt_app_get_language(void);
void rt_app_get_profile_list(char names[RT_MAX_PROFILE_NAMES][32], uint8_t *count);
bool rt_app_is_running(void);
