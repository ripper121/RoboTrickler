#include "app_config.h"

#include <string.h>

void rt_config_set_defaults(rt_config_t *config)
{
    memset(config, 0, sizeof(*config));
    strlcpy(config->scale_protocol, "GG", sizeof(config->scale_protocol));
    config->scale_baud = 9600;
    strlcpy(config->profile, "calibrate", sizeof(config->profile));
    config->target_weight = 40.0f;
    strlcpy(config->beeper, "done", sizeof(config->beeper));
    strlcpy(config->language, "en", sizeof(config->language));
    config->fw_check = true;
}
