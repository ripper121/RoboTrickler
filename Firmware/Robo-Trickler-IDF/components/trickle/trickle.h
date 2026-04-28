#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "actuator.h"
#include "esp_err.h"
#include "profile.h"
#include "scale.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t stepper1_steps;
    int32_t stepper2_steps;
    double stepper1_units;
    double stepper2_units;
    double remaining_units;
} trickle_result_t;

typedef bool (*trickle_should_stop_cb_t)(void *ctx);
typedef void (*trickle_progress_cb_t)(double applied_units, void *ctx);

typedef enum {
    TRICKLE_WARNING_RS232_TIMEOUT,
    TRICKLE_WARNING_RS232_ALARM,
    TRICKLE_WARNING_RS232_READ_FAIL,
} trickle_warning_t;

typedef void (*trickle_warning_cb_t)(trickle_warning_t warning, void *ctx);

typedef struct {
    actuator_handle_t         actuator;
    const app_profile_t      *profile;
    scale_handle_t            scale;
    trickle_should_stop_cb_t  should_stop;
    void                     *should_stop_ctx;
    trickle_progress_cb_t     progress;
    void                     *progress_ctx;
    trickle_warning_cb_t      warning;
    void                     *warning_ctx;
    bool                      done_beep_enabled;
    bool                      tare_auto;
    int                       tare_delay_ms;
} trickle_execute_config_t;

esp_err_t trickle_calculate(double trickle_value, const app_profile_t *profile, trickle_result_t *out_result);
bool trickle_is_running(void);
esp_err_t trickle_get_last_result(trickle_result_t *out_result, esp_err_t *out_status, bool *out_stopped);
esp_err_t trickle_get_last_scale_value(float *out_value, bool *out_valid);
esp_err_t trickle_execute(const trickle_execute_config_t *config,
                          double trickle_value,
                          double trickle_gap);
esp_err_t trickle_stop(void);

#ifdef __cplusplus
}
#endif
