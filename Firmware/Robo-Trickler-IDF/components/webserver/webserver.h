#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVER_DIR "/sdcard"

esp_err_t webserver_start(void);
esp_err_t webserver_stop(void);

#ifdef __cplusplus
}
#endif
