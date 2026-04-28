#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STEPPER_DEFAULT_FULL_STEPS_PER_REV 200U

typedef struct stepper_t *stepper_handle_t;

typedef enum {
    STEPPER_DIR_REVERSE = 0,
    STEPPER_DIR_FORWARD = 1,
} stepper_direction_t;

typedef struct {
    uint8_t step_bit;
    uint8_t dir_bit;
    uint8_t en_bit;
    uint32_t step_pulse_us;
    uint32_t min_step_low_us;
    uint32_t dir_setup_us;
} stepper_config_t;

esp_err_t stepper_init(const stepper_config_t *config, stepper_handle_t *out_handle);
esp_err_t stepper_move(stepper_handle_t handle, stepper_direction_t dir, uint32_t speed_rpm, uint32_t steps);
esp_err_t stepper_move_start(stepper_handle_t handle, stepper_direction_t dir, uint32_t speed_rpm, uint32_t steps);
esp_err_t stepper_is_running(stepper_handle_t handle, bool *out_running);
esp_err_t stepper_stop(stepper_handle_t handle);
double stepper_units_from_steps(int32_t steps, double units_per_throw);
esp_err_t stepper_calculate_steps_for_units(double remaining_units,
                                            double units_per_throw,
                                            int32_t *out_steps,
                                            double *out_units);

#ifdef __cplusplus
}
#endif
