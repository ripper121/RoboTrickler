#include "stepper.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "shift595.h"

static const char *TAG = "stepper";

#define STEPPER_TASK_STACK_SIZE 2048U
#define STEPPER_TASK_PRIORITY   5U
#define STEPPER_WAIT_SLICE_MS   5U

typedef struct {
    stepper_direction_t dir;
    uint32_t speed_rpm;
    uint32_t steps;
} stepper_move_request_t;

struct stepper_t {
    uint8_t step_bit;
    uint8_t dir_bit;
    uint8_t en_bit;
    uint32_t step_pulse_us;
    uint32_t min_step_low_us;
    uint32_t dir_setup_us;
    SemaphoreHandle_t done_sem;
    portMUX_TYPE lock;
    TaskHandle_t task;
    bool running;
    bool stop_requested;
    esp_err_t last_status;
    stepper_move_request_t request;
};

static esp_err_t stepper_validate_handle(stepper_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->done_sem != NULL, ESP_ERR_INVALID_STATE, TAG, "done sem is null");
    return ESP_OK;
}

static bool stepper_enabled_level(bool enabled)
{
    return !enabled;
}

static void stepper_set_running(stepper_handle_t handle, bool running)
{
    taskENTER_CRITICAL(&handle->lock);
    handle->running = running;
    if (!running) {
        handle->task = NULL;
        handle->stop_requested = false;
    }
    taskEXIT_CRITICAL(&handle->lock);
}

static bool stepper_get_stop_requested(stepper_handle_t handle)
{
    bool stop_requested = false;

    taskENTER_CRITICAL(&handle->lock);
    stop_requested = handle->stop_requested;
    taskEXIT_CRITICAL(&handle->lock);

    return stop_requested;
}

static esp_err_t stepper_rpm_to_period_us(uint32_t speed_rpm, uint32_t *out_period_us)
{
    uint64_t pulses_per_minute = 0;
    uint64_t speed_hz = 0;

    ESP_RETURN_ON_FALSE(out_period_us != NULL, ESP_ERR_INVALID_ARG, TAG, "out_period_us is null");
    ESP_RETURN_ON_FALSE(speed_rpm > 0U, ESP_ERR_INVALID_ARG, TAG, "speed_rpm must be > 0");

    pulses_per_minute = (uint64_t)speed_rpm * STEPPER_DEFAULT_FULL_STEPS_PER_REV;
    speed_hz = (pulses_per_minute + 30U) / 60U;
    ESP_RETURN_ON_FALSE(speed_hz > 0U && speed_hz <= UINT32_MAX, ESP_ERR_INVALID_ARG, TAG, "speed conversion overflow");

    *out_period_us = (uint32_t)((1000000ULL + speed_hz - 1ULL) / speed_hz);
    return ESP_OK;
}

static esp_err_t stepper_run_move(stepper_handle_t handle,
                                  stepper_direction_t dir,
                                  uint32_t speed_rpm,
                                  uint32_t steps)
{
    uint32_t period_us = 0;
    uint32_t low_us = 0;
    esp_err_t ret = ESP_OK;

    if (steps == 0U) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(stepper_rpm_to_period_us(speed_rpm, &period_us), TAG, "speed conversion failed");
    if (period_us < handle->step_pulse_us + handle->min_step_low_us) {
        period_us = handle->step_pulse_us + handle->min_step_low_us;
    }
    low_us = period_us - handle->step_pulse_us;

    ESP_RETURN_ON_ERROR(shift595_set_bit(handle->dir_bit, dir == STEPPER_DIR_FORWARD), TAG, "set direction failed");
    if (handle->dir_setup_us > 0U) {
        esp_rom_delay_us(handle->dir_setup_us);
    }
    ESP_RETURN_ON_ERROR(shift595_set_bit(handle->en_bit, stepper_enabled_level(true)), TAG, "enable stepper failed");

    for (uint32_t i = 0; i < steps; i++) {
        if (stepper_get_stop_requested(handle)) {
            ret = ESP_ERR_INVALID_STATE;
            break;
        }
        ret = shift595_pulse_bit(handle->step_bit, handle->step_pulse_us, low_us);
        if (ret != ESP_OK) {
            break;
        }
    }

    esp_err_t disable_ret = shift595_set_bit(handle->en_bit, stepper_enabled_level(false));
    return ret != ESP_OK ? ret : disable_ret;
}

static void stepper_task(void *arg)
{
    stepper_handle_t handle = (stepper_handle_t)arg;
    stepper_move_request_t request = handle->request;
    esp_err_t ret = stepper_run_move(handle, request.dir, request.speed_rpm, request.steps);

    taskENTER_CRITICAL(&handle->lock);
    handle->last_status = ret;
    taskEXIT_CRITICAL(&handle->lock);

    stepper_set_running(handle, false);
    xSemaphoreGive(handle->done_sem);
    vTaskDelete(NULL);
}

esp_err_t stepper_init(const stepper_config_t *config, stepper_handle_t *out_handle)
{
    stepper_handle_t handle = NULL;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(config->step_bit < 32U, ESP_ERR_INVALID_ARG, TAG, "step bit invalid");
    ESP_RETURN_ON_FALSE(config->dir_bit < 32U, ESP_ERR_INVALID_ARG, TAG, "dir bit invalid");
    ESP_RETURN_ON_FALSE(config->en_bit < 32U, ESP_ERR_INVALID_ARG, TAG, "enable bit invalid");

    handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory for stepper handle");

    handle->step_bit = config->step_bit;
    handle->dir_bit = config->dir_bit;
    handle->en_bit = config->en_bit;
    handle->step_pulse_us = config->step_pulse_us > 0U ? config->step_pulse_us : 2U;
    handle->min_step_low_us = config->min_step_low_us > 0U ? config->min_step_low_us : 2U;
    handle->dir_setup_us = config->dir_setup_us > 0U ? config->dir_setup_us : 1U;
    handle->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    handle->last_status = ESP_OK;
    handle->done_sem = xSemaphoreCreateBinary();
    if (handle->done_sem == NULL) {
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = shift595_init();
    if (ret == ESP_OK) {
        ret = shift595_set_bit(handle->step_bit, false);
    }
    if (ret == ESP_OK) {
        ret = shift595_set_bit(handle->dir_bit, false);
    }
    if (ret == ESP_OK) {
        ret = shift595_set_bit(handle->en_bit, stepper_enabled_level(false));
    }
    if (ret != ESP_OK) {
        vSemaphoreDelete(handle->done_sem);
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t stepper_move_start(stepper_handle_t handle, stepper_direction_t dir, uint32_t speed_rpm, uint32_t steps)
{
    ESP_RETURN_ON_ERROR(stepper_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(dir == STEPPER_DIR_FORWARD || dir == STEPPER_DIR_REVERSE,
                        ESP_ERR_INVALID_ARG, TAG, "direction invalid");

    if (steps == 0U) {
        return ESP_OK;
    }

    taskENTER_CRITICAL(&handle->lock);
    if (handle->running) {
        taskEXIT_CRITICAL(&handle->lock);
        return ESP_ERR_INVALID_STATE;
    }
    handle->running = true;
    handle->stop_requested = false;
    handle->last_status = ESP_OK;
    handle->request = (stepper_move_request_t) {
        .dir = dir,
        .speed_rpm = speed_rpm,
        .steps = steps,
    };
    taskEXIT_CRITICAL(&handle->lock);

    while (xSemaphoreTake(handle->done_sem, 0) == pdTRUE) {
    }

    if (xTaskCreate(stepper_task,
                    "stepper",
                    STEPPER_TASK_STACK_SIZE,
                    handle,
                    STEPPER_TASK_PRIORITY,
                    &handle->task) != pdPASS) {
        stepper_set_running(handle, false);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t stepper_is_running(stepper_handle_t handle, bool *out_running)
{
    ESP_RETURN_ON_ERROR(stepper_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(out_running != NULL, ESP_ERR_INVALID_ARG, TAG, "out_running is null");

    taskENTER_CRITICAL(&handle->lock);
    *out_running = handle->running;
    taskEXIT_CRITICAL(&handle->lock);
    return ESP_OK;
}

esp_err_t stepper_stop(stepper_handle_t handle)
{
    ESP_RETURN_ON_ERROR(stepper_validate_handle(handle), TAG, "invalid handle");

    taskENTER_CRITICAL(&handle->lock);
    handle->stop_requested = true;
    taskEXIT_CRITICAL(&handle->lock);

    for (;;) {
        bool running = false;

        ESP_RETURN_ON_ERROR(stepper_is_running(handle, &running), TAG, "running read failed");
        if (!running) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(STEPPER_WAIT_SLICE_MS));
    }

    return shift595_set_bit(handle->en_bit, stepper_enabled_level(false));
}

esp_err_t stepper_move(stepper_handle_t handle, stepper_direction_t dir, uint32_t speed_rpm, uint32_t steps)
{
    esp_err_t ret = ESP_OK;

    if (steps == 0U) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(stepper_move_start(handle, dir, speed_rpm, steps), TAG, "move start failed");
    xSemaphoreTake(handle->done_sem, portMAX_DELAY);

    taskENTER_CRITICAL(&handle->lock);
    ret = handle->last_status;
    taskEXIT_CRITICAL(&handle->lock);
    return ret == ESP_ERR_INVALID_STATE ? ESP_OK : ret;
}

double stepper_units_from_steps(int32_t steps, double units_per_throw)
{
    if (units_per_throw <= 0.0 || steps == 0) {
        return 0.0;
    }

    return ((double)(steps < 0 ? -steps : steps) * units_per_throw) /
           (double)STEPPER_DEFAULT_FULL_STEPS_PER_REV;
}

esp_err_t stepper_calculate_steps_for_units(double remaining_units,
                                            double units_per_throw,
                                            int32_t *out_steps,
                                            double *out_units)
{
    double steps_per_unit = 0.0;
    double exact_steps = 0.0;

    ESP_RETURN_ON_FALSE(out_steps != NULL, ESP_ERR_INVALID_ARG, TAG, "out_steps is null");
    ESP_RETURN_ON_FALSE(out_units != NULL, ESP_ERR_INVALID_ARG, TAG, "out_units is null");

    *out_steps = 0;
    *out_units = 0.0;

    if (units_per_throw <= 0.0 || remaining_units <= 0.0) {
        return ESP_OK;
    }

    steps_per_unit = (double)STEPPER_DEFAULT_FULL_STEPS_PER_REV / units_per_throw;
    exact_steps = remaining_units * steps_per_unit;
    ESP_RETURN_ON_FALSE(isfinite(exact_steps) && exact_steps >= 0.0 && exact_steps <= (double)INT32_MAX,
                        ESP_ERR_INVALID_ARG, TAG, "step count out of range");

    *out_steps = (int32_t)exact_steps;
    *out_units = ((double)(*out_steps) * units_per_throw) / (double)STEPPER_DEFAULT_FULL_STEPS_PER_REV;
    return ESP_OK;
}
