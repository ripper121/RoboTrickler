#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "stepper.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct actuator_t *actuator_handle_t;

typedef enum {
    ACTUATOR_ID_STEPPER1 = 0,
    ACTUATOR_ID_STEPPER2,
} actuator_id_t;

esp_err_t actuator_init(actuator_handle_t *out_handle);
esp_err_t actuator_move_stepper(actuator_handle_t handle,
                                actuator_id_t actuator_id,
                                stepper_direction_t dir,
                                uint32_t speed_rpm,
                                uint32_t steps);
esp_err_t actuator_start_stepper(actuator_handle_t handle,
                                 actuator_id_t actuator_id,
                                 stepper_direction_t dir,
                                 uint32_t speed_rpm,
                                 uint32_t steps);
esp_err_t actuator_stepper_is_running(actuator_handle_t handle, actuator_id_t actuator_id, bool *out_running);
esp_err_t actuator_stop_all(actuator_handle_t handle);
esp_err_t actuator_stop_stepper(actuator_handle_t handle, actuator_id_t actuator_id);
esp_err_t actuator_beep_ms(actuator_handle_t handle, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
