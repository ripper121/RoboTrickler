#include "trickle.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rs232.h"
#include "stepper.h"

static const char *TAG = "trickle";

#define TRICKLE_RS232_TX_TIMEOUT_MS 1000
#define TRICKLE_STOP_POLL_MS        10U
#define TRICKLE_RS232_RESTART_DELAY_MS 1000U
#define TRICKLE_RS232_RESTART_POLL_MS  50U
#define TRICKLE_DONE_BUZZER_MS         500U
#define TRICKLE_ALARM_BUZZER_MS        200U
#define TRICKLE_ALARM_BUZZER_PAUSE_MS  150U
#define TRICKLE_ALARM_BUZZER_COUNT     3U
#define TRICKLE_TASK_STACK_SIZE        6144U
#define TRICKLE_TASK_PRIORITY          5U
#define TRICKLE_SEND_RS232_VALUE       false

typedef struct {
    trickle_execute_config_t config;
    trickle_result_t last_result;
    TaskHandle_t task_handle;
    double trickle_value;
    double trickle_gap;
    esp_err_t last_status;
    float last_scale_value;
    bool last_scale_value_valid;
    bool running;
    bool stop_requested;
    bool last_stopped;
} trickle_async_state_t;

static trickle_async_state_t s_trickle_state;
static portMUX_TYPE s_trickle_lock = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t trickle_execute_sync(const trickle_execute_config_t *config,
                                      double trickle_value,
                                      double trickle_gap,
                                      trickle_result_t *out_result);
static esp_err_t trickle_execute_single_sync(const trickle_execute_config_t *config,
                                             double trickle_value,
                                             double trickle_gap,
                                             bool *io_rs232_alarm_reported,
                                             trickle_result_t *out_result);
static void trickle_task(void *arg);
static void trickle_set_last_scale_value(float value);

static void trickle_set_last_scale_value(float value)
{
    taskENTER_CRITICAL(&s_trickle_lock);
    s_trickle_state.last_scale_value = value;
    s_trickle_state.last_scale_value_valid = true;
    taskEXIT_CRITICAL(&s_trickle_lock);
}

static bool trickle_stop_requested(void)
{
    bool stop_requested = false;

    taskENTER_CRITICAL(&s_trickle_lock);
    stop_requested = s_trickle_state.stop_requested;
    taskEXIT_CRITICAL(&s_trickle_lock);

    return stop_requested;
}

static bool trickle_should_stop(const trickle_execute_config_t *config)
{
    if (trickle_stop_requested())
    {
        return true;
    }

    return config->should_stop != NULL && config->should_stop(config->should_stop_ctx);
}

static void trickle_stop_all_best_effort(const trickle_execute_config_t *config)
{
    esp_err_t ret = ESP_OK;

    if (config == NULL || config->actuator == NULL)
    {
        return;
    }

    ret = actuator_stop_all(config->actuator);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "actuator_stop_all failed during stop request: %s", esp_err_to_name(ret));
    }
}

static bool trickle_wait_with_stop(const trickle_execute_config_t *config, TickType_t delay_ticks)
{
    TickType_t remaining_ticks = delay_ticks;
    TickType_t poll_ticks = pdMS_TO_TICKS(TRICKLE_STOP_POLL_MS);

    if (poll_ticks == 0)
    {
        poll_ticks = 1;
    }

    while (remaining_ticks > 0)
    {
        TickType_t wait_ticks = remaining_ticks > poll_ticks ? poll_ticks : remaining_ticks;

        if (trickle_should_stop(config))
        {
            trickle_stop_all_best_effort(config);
            return false;
        }

        vTaskDelay(wait_ticks);
        remaining_ticks -= wait_ticks;
    }

    if (trickle_should_stop(config))
    {
        trickle_stop_all_best_effort(config);
        return false;
    }

    return true;
}

static void trickle_report_progress(const trickle_execute_config_t *config, double applied_units)
{
    if (config->progress != NULL)
    {
        config->progress(applied_units, config->progress_ctx);
    }
}

static void trickle_report_warning(const trickle_execute_config_t *config, trickle_warning_t warning)
{
    if (config != NULL && config->warning != NULL)
    {
        config->warning(warning, config->warning_ctx);
    }
}

static bool trickle_rs232_read_error_is_recoverable(esp_err_t ret)
{
    return ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NOT_FOUND || ret == ESP_ERR_INVALID_SIZE;
}

static void trickle_report_rs232_read_error(const trickle_execute_config_t *config,
                                            const char *context,
                                            esp_err_t ret)
{
    if (ret == ESP_ERR_TIMEOUT)
    {
        ESP_LOGW(TAG, "%s timed out, retrying", context);
        trickle_report_warning(config, TRICKLE_WARNING_RS232_TIMEOUT);
        return;
    }

    ESP_LOGW(TAG, "%s returned no usable scale value (%s), retrying", context, esp_err_to_name(ret));
    trickle_report_warning(config, TRICKLE_WARNING_RS232_READ_FAIL);
}

static bool trickle_rs232_alarm_active(const trickle_execute_config_t *config,
                                       double scale_value,
                                       double target_weight)
{
    double alarm_threshold = 0.0;

    if (config == NULL || config->profile == NULL)
    {
        return false;
    }

    alarm_threshold = config->profile->general.alarm_threshold;
    if (!isfinite(scale_value) || !isfinite(target_weight) || !isfinite(alarm_threshold))
    {
        return false;
    }
    if (alarm_threshold < 0.0)
    {
        alarm_threshold = 0.0;
    }

    return scale_value > (target_weight + alarm_threshold);
}

static void trickle_beep_alarm(const trickle_execute_config_t *config)
{
    TickType_t pause_ticks = pdMS_TO_TICKS(TRICKLE_ALARM_BUZZER_PAUSE_MS);

    if (config == NULL || config->actuator == NULL || !config->done_beep_enabled)
    {
        return;
    }

    if (pause_ticks == 0)
    {
        pause_ticks = 1;
    }

    for (uint32_t i = 0; i < TRICKLE_ALARM_BUZZER_COUNT; ++i)
    {
        esp_err_t beep_ret = actuator_beep_ms(config->actuator, TRICKLE_ALARM_BUZZER_MS);

        if (beep_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "alarm beep failed: %s", esp_err_to_name(beep_ret));
            return;
        }

        if ((i + 1U) < TRICKLE_ALARM_BUZZER_COUNT)
        {
            vTaskDelay(pause_ticks);
        }
    }
}

static void trickle_check_rs232_alarm(const trickle_execute_config_t *config,
                                      double scale_value,
                                      double target_weight,
                                      bool *io_alarm_reported)
{
    double alarm_threshold = 0.0;

    if (io_alarm_reported != NULL && *io_alarm_reported)
    {
        return;
    }

    if (config == NULL || config->profile == NULL)
    {
        return;
    }

    alarm_threshold = config->profile->general.alarm_threshold;
    if (!isfinite(scale_value) || !isfinite(target_weight) || !isfinite(alarm_threshold))
    {
        return;
    }
    if (alarm_threshold < 0.0)
    {
        alarm_threshold = 0.0;
    }

    if (trickle_rs232_alarm_active(config, scale_value, target_weight))
    {
        ESP_LOGW(TAG,
                 "RS232 alarm: scale value %.3f exceeds target %.3f + threshold %.3f",
                 scale_value,
                 target_weight,
                 alarm_threshold);
        if (io_alarm_reported != NULL)
        {
            *io_alarm_reported = true;
        }
        trickle_report_warning(config, TRICKLE_WARNING_RS232_ALARM);
        trickle_beep_alarm(config);
    }
}

static void trickle_beep_done(const trickle_execute_config_t *config)
{
    esp_err_t beep_ret = ESP_OK;

    if (config == NULL || config->actuator == NULL)
    {
        return;
    }

    beep_ret = actuator_beep_ms(config->actuator, TRICKLE_DONE_BUZZER_MS);
    if (beep_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "actuator_beep_ms failed: %s", esp_err_to_name(beep_ret));
    }
}

static double trickle_stepper_units_from_steps(int32_t steps, const profile_stepper_actuator_t *actuator)
{
    if (actuator == NULL || actuator->units_per_throw <= 0.0 || steps == 0)
    {
        return 0.0;
    }

    return stepper_units_from_steps(steps, actuator->units_per_throw);
}

static const char *trickle_profile_actuator_name(const profile_trickle_step_t *step)
{
    if (step == NULL || !step->has_actuator)
    {
        return "unknown";
    }

    switch (step->actuator)
    {
    case ACTUATOR_ID_STEPPER1:
        return "stepper1";
    case ACTUATOR_ID_STEPPER2:
        return "stepper2";
    default:
        return "unknown";
    }
}

static esp_err_t trickle_calculate_stepper(double remaining_units,
                                           const profile_stepper_actuator_t *actuator,
                                           int32_t *out_steps,
                                           double *out_units)
{
    ESP_RETURN_ON_FALSE(actuator != NULL, ESP_ERR_INVALID_ARG, TAG, "actuator is null");
    ESP_RETURN_ON_FALSE(out_steps != NULL, ESP_ERR_INVALID_ARG, TAG, "out_steps is null");
    ESP_RETURN_ON_FALSE(out_units != NULL, ESP_ERR_INVALID_ARG, TAG, "out_units is null");

    if (!actuator->enabled || actuator->units_per_throw <= 0.0 || remaining_units <= 0.0)
    {
        *out_steps = 0;
        *out_units = 0.0;
        return ESP_OK;
    }

    return stepper_calculate_steps_for_units(remaining_units,
                                             actuator->units_per_throw,
                                             out_steps,
                                             out_units);
}

static esp_err_t trickle_execute_rs232_step(const trickle_execute_config_t *config,
                                            const profile_trickle_step_t *step)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(step != NULL, ESP_ERR_INVALID_ARG, TAG, "step is null");
    ESP_RETURN_ON_FALSE(step->has_actuator, ESP_ERR_NOT_SUPPORTED, TAG, "rs232 trickle actuator is invalid");

    if (step->steps <= 0)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_RETURN_ON_FALSE(step->actuator == ACTUATOR_ID_STEPPER1 || step->actuator == ACTUATOR_ID_STEPPER2,
                        ESP_ERR_NOT_SUPPORTED, TAG, "rs232 trickle actuator is not a stepper");
    ESP_RETURN_ON_FALSE(step->speed > 0, ESP_ERR_INVALID_ARG, TAG, "stepper speed must be > 0");

    return actuator_move_stepper(config->actuator,
                                 step->actuator,
                                 step->reverse ? STEPPER_DIR_REVERSE : STEPPER_DIR_FORWARD,
                                 (uint32_t)step->speed,
                                 (uint32_t)step->steps);
}

static esp_err_t rt_trickle_send_rs232_value(double value)
{
    #if TRICKLE_SEND_RS232_VALUE
        char buffer[32];
        int len = 0;
        int written = 0;

        ESP_RETURN_ON_FALSE(isfinite(value), ESP_ERR_INVALID_ARG, TAG, "value must be finite");

        len = snprintf(buffer, sizeof(buffer), "value: %.3f\r\n", value);
        ESP_RETURN_ON_FALSE(len > 0 && len < (int)sizeof(buffer), ESP_ERR_INVALID_SIZE, TAG, "rs232 payload too long");

        written = uart_write_bytes(RS232_DEFAULT_UART_PORT, buffer, (size_t)len);
        ESP_RETURN_ON_FALSE(written == len, ESP_FAIL, TAG, "short uart write");
        //ESP_RETURN_ON_ERROR(uart_wait_tx_done(RS232_DEFAULT_UART_PORT, pdMS_TO_TICKS(TRICKLE_RS232_TX_TIMEOUT_MS)), TAG, "uart_wait_tx_done failed");
    #endif

    return ESP_OK;
}

esp_err_t trickle_calculate(double trickle_value, const app_profile_t *profile, trickle_result_t *out_result)
{
    trickle_result_t result = {0};
    double remaining_units = trickle_value;

    ESP_RETURN_ON_FALSE(profile != NULL, ESP_ERR_INVALID_ARG, TAG, "profile is null");
    ESP_RETURN_ON_FALSE(out_result != NULL, ESP_ERR_INVALID_ARG, TAG, "out_result is null");
    ESP_RETURN_ON_FALSE(isfinite(trickle_value), ESP_ERR_INVALID_ARG, TAG, "trickle_value must be finite");

    if (remaining_units < 0.0)
    {
        remaining_units = 0.0;
    }

    ESP_LOGI(TAG,
             "calc in value=%.3f stepper1=%d/%.3f stepper2=%d/%.3f",
             trickle_value,
             profile->actuator.stepper1.enabled,
             profile->actuator.stepper1.units_per_throw,
             profile->actuator.stepper2.enabled,
             profile->actuator.stepper2.units_per_throw);

    ESP_RETURN_ON_ERROR(trickle_calculate_stepper(remaining_units,
                                                  &profile->actuator.stepper2,
                                                  &result.stepper2_steps,
                                                  &result.stepper2_units),
                        TAG, "calculate stepper2 failed");
    remaining_units -= result.stepper2_units;

    ESP_RETURN_ON_ERROR(trickle_calculate_stepper(remaining_units,
                                                  &profile->actuator.stepper1,
                                                  &result.stepper1_steps,
                                                  &result.stepper1_units),
                        TAG, "calculate stepper1 failed");
    remaining_units -= result.stepper1_units;

    if (remaining_units < 0.0)
    {
        remaining_units = 0.0;
    }

    result.remaining_units = remaining_units;
    *out_result = result;

    ESP_LOGI(TAG,
             "calc out stepper2_steps=%ld stepper2_units=%.3f stepper1_steps=%ld stepper1_units=%.3f rest=%.3f",
             (long)result.stepper2_steps,
             result.stepper2_units,
             (long)result.stepper1_steps,
             result.stepper1_units,
             result.remaining_units);

    return ESP_OK;
}

bool trickle_is_running(void)
{
    bool running = false;

    taskENTER_CRITICAL(&s_trickle_lock);
    running = s_trickle_state.running;
    taskEXIT_CRITICAL(&s_trickle_lock);

    return running;
}

esp_err_t trickle_get_last_result(trickle_result_t *out_result, esp_err_t *out_status, bool *out_stopped)
{
    taskENTER_CRITICAL(&s_trickle_lock);
    if (out_result != NULL)
    {
        *out_result = s_trickle_state.last_result;
    }
    if (out_status != NULL)
    {
        *out_status = s_trickle_state.last_status;
    }
    if (out_stopped != NULL)
    {
        *out_stopped = s_trickle_state.last_stopped;
    }
    taskEXIT_CRITICAL(&s_trickle_lock);

    return ESP_OK;
}

esp_err_t trickle_get_last_scale_value(float *out_value, bool *out_valid)
{
    taskENTER_CRITICAL(&s_trickle_lock);
    if (out_value != NULL)
    {
        *out_value = s_trickle_state.last_scale_value;
    }
    if (out_valid != NULL)
    {
        *out_valid = s_trickle_state.last_scale_value_valid;
    }
    taskEXIT_CRITICAL(&s_trickle_lock);

    return ESP_OK;
}

esp_err_t trickle_execute(const trickle_execute_config_t *config,
                          double trickle_value,
                          double trickle_gap)
{
    BaseType_t task_created = pdFALSE;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(config->actuator != NULL, ESP_ERR_INVALID_ARG, TAG, "actuator is null");
    ESP_RETURN_ON_FALSE(config->profile != NULL, ESP_ERR_INVALID_ARG, TAG, "profile is null");
    ESP_RETURN_ON_FALSE(isfinite(trickle_value), ESP_ERR_INVALID_ARG, TAG, "trickle_value must be finite");
    ESP_RETURN_ON_FALSE(isfinite(trickle_gap), ESP_ERR_INVALID_ARG, TAG, "trickle_gap must be finite");

    taskENTER_CRITICAL(&s_trickle_lock);
    if (s_trickle_state.running)
    {
        taskEXIT_CRITICAL(&s_trickle_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_trickle_state.config = *config;
    s_trickle_state.trickle_value = trickle_value;
    s_trickle_state.trickle_gap = trickle_gap;
    s_trickle_state.last_result = (trickle_result_t){0};
    s_trickle_state.last_status = ESP_OK;
    s_trickle_state.last_scale_value = 0.0f;
    s_trickle_state.last_scale_value_valid = false;
    s_trickle_state.last_stopped = false;
    s_trickle_state.stop_requested = false;
    s_trickle_state.running = true;
    s_trickle_state.task_handle = NULL;
    taskEXIT_CRITICAL(&s_trickle_lock);

    task_created = xTaskCreate(trickle_task, "trickle", TRICKLE_TASK_STACK_SIZE, NULL, TRICKLE_TASK_PRIORITY, NULL);
    if (task_created != pdPASS)
    {
        taskENTER_CRITICAL(&s_trickle_lock);
        s_trickle_state.running = false;
        s_trickle_state.stop_requested = false;
        taskEXIT_CRITICAL(&s_trickle_lock);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t trickle_stop(void)
{
    actuator_handle_t actuator = NULL;

    taskENTER_CRITICAL(&s_trickle_lock);
    if (!s_trickle_state.running)
    {
        taskEXIT_CRITICAL(&s_trickle_lock);
        return ESP_ERR_INVALID_STATE;
    }

    s_trickle_state.stop_requested = true;
    actuator = s_trickle_state.config.actuator;
    taskEXIT_CRITICAL(&s_trickle_lock);

    if (actuator != NULL)
    {
        return actuator_stop_all(actuator);
    }

    return ESP_OK;
}

static void trickle_task(void *arg)
{
    trickle_execute_config_t config;
    trickle_result_t result = {0};
    esp_err_t ret = ESP_OK;
    bool stopped = false;

    (void)arg;

    taskENTER_CRITICAL(&s_trickle_lock);
    config = s_trickle_state.config;
    s_trickle_state.task_handle = xTaskGetCurrentTaskHandle();
    taskEXIT_CRITICAL(&s_trickle_lock);

    ret = trickle_execute_sync(&config, s_trickle_state.trickle_value, s_trickle_state.trickle_gap, &result);
    stopped = trickle_should_stop(&config);

    taskENTER_CRITICAL(&s_trickle_lock);
    s_trickle_state.last_result = result;
    s_trickle_state.last_status = ret;
    s_trickle_state.last_stopped = stopped;
    s_trickle_state.running = false;
    s_trickle_state.stop_requested = false;
    s_trickle_state.task_handle = NULL;
    taskEXIT_CRITICAL(&s_trickle_lock);

    vTaskDelete(NULL);
}

static esp_err_t trickle_execute_sync(const trickle_execute_config_t *config,
                                      double trickle_value,
                                      double trickle_gap,
                                      trickle_result_t *out_result)
{
    trickle_result_t total_result = {0};
    float scale_value = 0.0f;
    esp_err_t ret = ESP_OK;
    bool rs232_alarm_reported = false;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(config->actuator != NULL, ESP_ERR_INVALID_ARG, TAG, "actuator is null");
    ESP_RETURN_ON_FALSE(config->profile != NULL, ESP_ERR_INVALID_ARG, TAG, "profile is null");
    ESP_RETURN_ON_FALSE(isfinite(trickle_value), ESP_ERR_INVALID_ARG, TAG, "trickle_value must be finite");
    ESP_RETURN_ON_FALSE(isfinite(trickle_gap), ESP_ERR_INVALID_ARG, TAG, "trickle_gap must be finite");

    for (;;)
    {
        trickle_result_t single_result = {0};
        bool saw_sub_zero = false;

        if (trickle_should_stop(config))
        {
            break;
        }

        if (config->scale != NULL &&
            config->tare_auto)
        {
            TickType_t tare_delay_ticks = pdMS_TO_TICKS(config->tare_delay_ms > 0 ?
                                                        config->tare_delay_ms :
                                                        0);

            ret = scale_tare(config->scale);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "RS232 auto tare failed: %s", esp_err_to_name(ret));
                goto done;
            }                                            

            if (!trickle_wait_with_stop(config, tare_delay_ticks))
            {
                break;
            }
        }

        ret = trickle_execute_single_sync(config,
                                          trickle_value,
                                          trickle_gap,
                                          &rs232_alarm_reported,
                                          &single_result);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "trickle_execute_single_sync failed: %s", esp_err_to_name(ret));
            goto done;
        }

        total_result.stepper1_steps += single_result.stepper1_steps;
        total_result.stepper2_steps += single_result.stepper2_steps;
        total_result.stepper1_units += single_result.stepper1_units;
        total_result.stepper2_units += single_result.stepper2_units;
        total_result.remaining_units = single_result.remaining_units;

        if (config->scale == NULL ||
            config->profile->rs232_trickle_count == 0U)
        {
            break;
        }

        for (;;)
        {
            if (trickle_should_stop(config))
            {
                goto done;
            }

            if (!trickle_wait_with_stop(config, pdMS_TO_TICKS(TRICKLE_RS232_RESTART_DELAY_MS)))
            {
                goto done;
            }

            ret = scale_read_unit_stable(config->scale,
                                         profile_trickle_measurements(&config->profile->rs232_trickle[0]),
                                         &scale_value);
            if (trickle_rs232_read_error_is_recoverable(ret))
            {
                trickle_report_rs232_read_error(config, "Post-trickle RS232 scale read", ret);
                continue;
            }
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "post-trickle stable scale read failed: %s", esp_err_to_name(ret));
                goto done;
            }

            trickle_set_last_scale_value(scale_value);
            ESP_LOGI(TAG, "Post-trickle RS232 stable scale value=%.3f", (double)scale_value);
            trickle_check_rs232_alarm(config, (double)scale_value, trickle_value, &rs232_alarm_reported);

            if ((double)scale_value < 0.0)
            {
                saw_sub_zero = true;
            }
            else if (saw_sub_zero && (double)scale_value >= 0.0)
            {
                ESP_LOGI(TAG, "Restarting RS232 trickle after sub-zero recovery %.3f", (double)scale_value);
                break;
            }

            if (!trickle_wait_with_stop(config, pdMS_TO_TICKS(TRICKLE_RS232_RESTART_POLL_MS)))
            {
                goto done;
            }
        }
    }

done:
    if (out_result != NULL)
    {
        *out_result = total_result;
    }
    return ret;
}

static esp_err_t trickle_execute_single_sync(const trickle_execute_config_t *config,
                                             double trickle_value,
                                             double trickle_gap,
                                             bool *io_rs232_alarm_reported,
                                             trickle_result_t *out_result)
{
    trickle_result_t planned_result = {0};
    trickle_result_t executed_result = {0};
    double adjusted_trickle_value = 0.0;
    float scale_value = 0.0f;
    esp_err_t ret;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(config->actuator != NULL, ESP_ERR_INVALID_ARG, TAG, "actuator is null");
    ESP_RETURN_ON_FALSE(config->profile != NULL, ESP_ERR_INVALID_ARG, TAG, "profile is null");
    ESP_RETURN_ON_FALSE(isfinite(trickle_value), ESP_ERR_INVALID_ARG, TAG, "trickle_value must be finite");
    ESP_RETURN_ON_FALSE(isfinite(trickle_gap), ESP_ERR_INVALID_ARG, TAG, "trickle_gap must be finite");

    adjusted_trickle_value = trickle_value - trickle_gap;
    if (adjusted_trickle_value < 0.0)
    {
        adjusted_trickle_value = 0.0;
    }

    ESP_RETURN_ON_ERROR(trickle_calculate(adjusted_trickle_value, config->profile, &planned_result), TAG, "trickle_calculate failed");

    if (planned_result.stepper2_steps > 0)
    {
        if (trickle_should_stop(config))
        {
            goto done;
        }

        ESP_LOGI(TAG, "Executing stepper2 trickle: %ld steps", (long)planned_result.stepper2_steps);
        ESP_RETURN_ON_ERROR(actuator_move_stepper(config->actuator,
                                                  ACTUATOR_ID_STEPPER2,
                                                  STEPPER_DIR_FORWARD,
                                                  (uint32_t)config->profile->actuator.stepper2.units_per_throw_speed,
                                                  (uint32_t)planned_result.stepper2_steps),
                            TAG, "stepper2 trickle failed");
        if (!trickle_should_stop(config)) {
            executed_result.stepper2_steps = planned_result.stepper2_steps;
            executed_result.stepper2_units =
                trickle_stepper_units_from_steps(planned_result.stepper2_steps, &config->profile->actuator.stepper2);
        }
        if (config->scale != NULL)
        {
            ESP_RETURN_ON_ERROR(rt_trickle_send_rs232_value(executed_result.stepper2_units),
                                TAG, "send stepper2 units over rs232 failed");
        }
        trickle_report_progress(config, executed_result.stepper2_units);
    }

    if (planned_result.stepper1_steps > 0)
    {
        if (trickle_should_stop(config))
        {
            goto done;
        }

        ESP_LOGI(TAG, "Executing stepper1 trickle: %ld steps", (long)planned_result.stepper1_steps);
        ESP_RETURN_ON_ERROR(actuator_move_stepper(config->actuator,
                                                  ACTUATOR_ID_STEPPER1,
                                                  STEPPER_DIR_FORWARD,
                                                  (uint32_t)config->profile->actuator.stepper1.units_per_throw_speed,
                                                  (uint32_t)planned_result.stepper1_steps),
                            TAG, "stepper1 trickle failed");
        if (!trickle_should_stop(config)) {
            executed_result.stepper1_steps = planned_result.stepper1_steps;
            executed_result.stepper1_units =
                trickle_stepper_units_from_steps(planned_result.stepper1_steps, &config->profile->actuator.stepper1);
        }
        if (config->scale != NULL)
        {
            ESP_RETURN_ON_ERROR(rt_trickle_send_rs232_value(executed_result.stepper1_units),
                                TAG, "send stepper1 units over rs232 failed");
        }
        trickle_report_progress(config, executed_result.stepper1_units);
    }

    if (config->scale != NULL &&
        config->profile->rs232_trickle_count > 0)
    {
        size_t selected_index = 0;
        for (;;)
        {
            const profile_trickle_step_t *selected_step = NULL;

            if (trickle_should_stop(config))
            {
                goto done;
            }

            ret = scale_read_unit_stable(config->scale,
                                         profile_trickle_measurements(&config->profile->rs232_trickle[selected_index]),
                                         &scale_value);
            if (trickle_rs232_read_error_is_recoverable(ret))
            {
                trickle_report_rs232_read_error(config, "RS232 scale read", ret);
                if (!trickle_wait_with_stop(config, pdMS_TO_TICKS(TRICKLE_RS232_RESTART_POLL_MS)))
                {
                    goto done;
                }
                continue;
            }
            if (ret != ESP_OK)
            {
                return ret;
            }

            trickle_set_last_scale_value(scale_value);
            ESP_LOGI(TAG, "RS232 scale value=%.3f target=%.3f", (double)scale_value, trickle_value);
            trickle_check_rs232_alarm(config, (double)scale_value, trickle_value, io_rs232_alarm_reported);
            if ((double)scale_value >= trickle_value)
            {
                const bool alarm_active = trickle_rs232_alarm_active(config, (double)scale_value, trickle_value);

                if (config->done_beep_enabled && !alarm_active)
                {
                    trickle_beep_done(config);
                }
                break;
            }

            for (size_t i = 0; i < config->profile->rs232_trickle_count; ++i)
            {
                const profile_trickle_step_t *candidate = &config->profile->rs232_trickle[i];

                if ((double)scale_value <= (trickle_value - candidate->diff_weight))
                {
                    selected_step = candidate;
                    selected_index = i;
                    break;
                }
            }

            if (selected_step == NULL)
            {
                ESP_LOGI(TAG, "No RS232 trickle step applies at scale value %.3f", (double)scale_value);
                break;
            }

            ESP_LOGI(TAG,
                     "RS232 trickle step %u: diffWeight=%.3f actuator=%s speed=%d steps=%d time=%.3f measurements=%d",
                     (unsigned)selected_index,
                     selected_step->diff_weight,
                     trickle_profile_actuator_name(selected_step),
                     selected_step->speed,
                     selected_step->steps,
                     selected_step->time,
                     selected_step->measurements);

            if (trickle_should_stop(config))
            {
                goto done;
            }

            {
                ret = trickle_execute_rs232_step(config, selected_step);
                if (ret == ESP_ERR_NOT_SUPPORTED)
                {
                    ESP_LOGW(TAG, "Skipping unsupported RS232 trickle step %u", (unsigned)selected_index);
                    break;
                }
                ESP_RETURN_ON_ERROR(ret, TAG, "rs232 trickle step failed");

                if (selected_step->actuator == ACTUATOR_ID_STEPPER2 && selected_step->steps > 0)
                {
                    double units = 0.0;

                    if (!trickle_should_stop(config)) {
                        units = trickle_stepper_units_from_steps((int32_t)selected_step->steps,
                                                                 &config->profile->actuator.stepper2);
                    }

                    executed_result.stepper2_steps += trickle_should_stop(config) ? 0 : selected_step->steps;
                    executed_result.stepper2_units += units;
                    if (config->scale != NULL)
                    {
                        ESP_RETURN_ON_ERROR(rt_trickle_send_rs232_value(units),
                                            TAG, "send stepper2 units over rs232 failed");
                    }
                    trickle_report_progress(config, units);
                }
                else if (selected_step->actuator == ACTUATOR_ID_STEPPER1 && selected_step->steps > 0)
                {
                    double units = 0.0;

                    if (!trickle_should_stop(config)) {
                        units = trickle_stepper_units_from_steps((int32_t)selected_step->steps,
                                                                 &config->profile->actuator.stepper1);
                    }

                    executed_result.stepper1_steps += trickle_should_stop(config) ? 0 : selected_step->steps;
                    executed_result.stepper1_units += units;
                    if (config->scale != NULL)
                    {
                        ESP_RETURN_ON_ERROR(rt_trickle_send_rs232_value(units),
                                            TAG, "send stepper1 units over rs232 failed");
                    }
                    trickle_report_progress(config, units);
                }
            }
        }
    }

done:
    executed_result.remaining_units = adjusted_trickle_value -
                                      executed_result.stepper2_units -
                                      executed_result.stepper1_units;
    if (executed_result.remaining_units < 0.0)
    {
        executed_result.remaining_units = 0.0;
    }
    if (out_result != NULL)
    {
        *out_result = executed_result;
    }
    return ESP_OK;
}
