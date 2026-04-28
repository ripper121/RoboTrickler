#include "actuator.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pinConfig.h"
#include "shift595.h"

static const char *TAG = "actuator";

struct actuator_t {
    stepper_handle_t stepper1;
    stepper_handle_t stepper2;
};

static esp_err_t actuator_validate_handle(actuator_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

static esp_err_t actuator_get_stepper(actuator_handle_t handle,
                                      actuator_id_t actuator_id,
                                      stepper_handle_t *out_stepper)
{
    ESP_RETURN_ON_ERROR(actuator_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(out_stepper != NULL, ESP_ERR_INVALID_ARG, TAG, "out_stepper is null");

    switch (actuator_id) {
    case ACTUATOR_ID_STEPPER1:
        *out_stepper = handle->stepper1;
        return ESP_OK;
    case ACTUATOR_ID_STEPPER2:
        *out_stepper = handle->stepper2;
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t actuator_init(actuator_handle_t *out_handle)
{
    actuator_handle_t handle = NULL;
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;

    handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory for actuator handle");

    ret = shift595_init();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "shift register init failed");

    const stepper_config_t stepper1_config = {
        .step_bit = PINCFG_SHIFT_X_STEP_BIT,
        .dir_bit = PINCFG_SHIFT_X_DIR_BIT,
        .en_bit = PINCFG_SHIFT_X_ENABLE_BIT,
        .step_pulse_us = 2,
        .min_step_low_us = 2,
        .dir_setup_us = 1,
    };
    const stepper_config_t stepper2_config = {
        .step_bit = PINCFG_SHIFT_Y_STEP_BIT,
        .dir_bit = PINCFG_SHIFT_Y_DIR_BIT,
        .en_bit = PINCFG_SHIFT_Y_ENABLE_BIT,
        .step_pulse_us = 2,
        .min_step_low_us = 2,
        .dir_setup_us = 1,
    };

    ESP_GOTO_ON_ERROR(stepper_init(&stepper1_config, &handle->stepper1), err, TAG, "stepper1 init failed");
    ESP_GOTO_ON_ERROR(stepper_init(&stepper2_config, &handle->stepper2), err, TAG, "stepper2 init failed");
    ESP_GOTO_ON_ERROR(shift595_set_bit(PINCFG_SHIFT_BUZZER_BIT, false), err, TAG, "buzzer init failed");

    *out_handle = handle;
    return ESP_OK;

err:
    free(handle);
    return ret;
}

esp_err_t actuator_move_stepper(actuator_handle_t handle,
                                actuator_id_t actuator_id,
                                stepper_direction_t dir,
                                uint32_t speed_rpm,
                                uint32_t steps)
{
    stepper_handle_t stepper = NULL;

    ESP_RETURN_ON_ERROR(actuator_get_stepper(handle, actuator_id, &stepper), TAG, "get stepper failed");
    ESP_RETURN_ON_FALSE(stepper != NULL, ESP_ERR_INVALID_STATE, TAG, "stepper not initialized");

    return stepper_move(stepper, dir, speed_rpm, steps);
}

esp_err_t actuator_start_stepper(actuator_handle_t handle,
                                 actuator_id_t actuator_id,
                                 stepper_direction_t dir,
                                 uint32_t speed_rpm,
                                 uint32_t steps)
{
    stepper_handle_t stepper = NULL;

    ESP_RETURN_ON_ERROR(actuator_get_stepper(handle, actuator_id, &stepper), TAG, "get stepper failed");
    ESP_RETURN_ON_FALSE(stepper != NULL, ESP_ERR_INVALID_STATE, TAG, "stepper not initialized");

    return stepper_move_start(stepper, dir, speed_rpm, steps);
}

esp_err_t actuator_stepper_is_running(actuator_handle_t handle, actuator_id_t actuator_id, bool *out_running)
{
    stepper_handle_t stepper = NULL;

    ESP_RETURN_ON_FALSE(out_running != NULL, ESP_ERR_INVALID_ARG, TAG, "out_running is null");
    ESP_RETURN_ON_ERROR(actuator_get_stepper(handle, actuator_id, &stepper), TAG, "get stepper failed");

    if (stepper == NULL) {
        *out_running = false;
        return ESP_OK;
    }

    return stepper_is_running(stepper, out_running);
}

esp_err_t actuator_stop_stepper(actuator_handle_t handle, actuator_id_t actuator_id)
{
    stepper_handle_t stepper = NULL;

    ESP_RETURN_ON_ERROR(actuator_get_stepper(handle, actuator_id, &stepper), TAG, "get stepper failed");
    if (stepper == NULL) {
        return ESP_OK;
    }

    return stepper_stop(stepper);
}

esp_err_t actuator_stop_all(actuator_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    esp_err_t stop_ret = ESP_OK;

    ESP_RETURN_ON_ERROR(actuator_validate_handle(handle), TAG, "invalid handle");

    stop_ret = actuator_stop_stepper(handle, ACTUATOR_ID_STEPPER1);
    if (stop_ret != ESP_OK) {
        ret = stop_ret;
    }
    stop_ret = actuator_stop_stepper(handle, ACTUATOR_ID_STEPPER2);
    if (stop_ret != ESP_OK && ret == ESP_OK) {
        ret = stop_ret;
    }

    return ret;
}

esp_err_t actuator_beep_ms(actuator_handle_t handle, uint32_t duration_ms)
{
    TickType_t delay_ticks = pdMS_TO_TICKS(duration_ms);

    ESP_RETURN_ON_ERROR(actuator_validate_handle(handle), TAG, "invalid handle");

    if (duration_ms == 0U) {
        return ESP_OK;
    }
    if (delay_ticks == 0) {
        delay_ticks = 1;
    }

    ESP_RETURN_ON_ERROR(shift595_set_bit(PINCFG_SHIFT_BUZZER_BIT, true), TAG, "buzzer on failed");
    vTaskDelay(delay_ticks);
    return shift595_set_bit(PINCFG_SHIFT_BUZZER_BIT, false);
}
