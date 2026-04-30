#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RT_MAX_PROFILE_STEPS 16
#define RT_MAX_PROFILE_NAMES 32
#define RT_MAX_TARGET_WEIGHT 999.0f

typedef struct {
    char wifi_ssid[64];
    char wifi_psk[64];
    char ip_static[16];
    char ip_gateway[16];
    char ip_subnet[16];
    char ip_dns[16];
    char beeper[16];
    char language[8];
    bool fw_check;
    float target_weight;
    char scale_protocol[32];
    int scale_baud;
    char scale_custom_code[32];
    char profile[32];

    uint8_t profile_num[RT_MAX_PROFILE_STEPS];
    float profile_weight[RT_MAX_PROFILE_STEPS];
    float profile_tolerance;
    float profile_alarm_threshold;
    float profile_weight_gap;
    int profile_general_measurements;
    double profile_stepper_units_per_throw[3];
    int profile_stepper_units_per_throw_speed[3];
    bool profile_stepper_enabled[3];
    int profile_measurements[RT_MAX_PROFILE_STEPS];
    long profile_steps[RT_MAX_PROFILE_STEPS];
    int profile_speed[RT_MAX_PROFILE_STEPS];
    bool profile_reverse[RT_MAX_PROFILE_STEPS];
    int profile_count;
} rt_config_t;

void rt_config_set_defaults(rt_config_t *config);
