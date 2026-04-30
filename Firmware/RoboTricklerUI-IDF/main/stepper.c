#include "stepper.h"

#include <stdlib.h>
#include "board.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RT_MOTOR_STEPS 200
#define RT_FULL_STEP 1

typedef struct {
    uint8_t dir;
    uint8_t step;
    int rpm;
} rt_stepper_t;

static const char *TAG = "rt_stepper";
static rt_stepper_t s_steppers[3] = {
    {0, 0, 0},
    {RT_I2S_X_DIRECTION_PIN, RT_I2S_X_STEP_PIN, 100},
    {RT_I2S_Y_DIRECTION_PIN, RT_I2S_Y_STEP_PIN, 100},
};
static uint32_t s_shift_state;
static portMUX_TYPE s_shift_mux = portMUX_INITIALIZER_UNLOCKED;

static void shift_flush_locked(void)
{
    gpio_set_level(RT_I2S_OUT_WS, 0);
    for (int bit = 0; bit < RT_I2S_OUT_NUM_BITS; bit++) {
        uint32_t mask = 1UL << (RT_I2S_OUT_NUM_BITS - 1 - bit);
        gpio_set_level(RT_I2S_OUT_DATA, (s_shift_state & mask) != 0);
        gpio_set_level(RT_I2S_OUT_BCK, 1);
        gpio_set_level(RT_I2S_OUT_BCK, 0);
    }
    gpio_set_level(RT_I2S_OUT_WS, 1);
}

static void shift_write(uint8_t pin, bool level)
{
    if (pin >= RT_I2S_OUT_NUM_BITS) {
        return;
    }
    portENTER_CRITICAL(&s_shift_mux);
    if (level) {
        s_shift_state |= 1UL << pin;
    } else {
        s_shift_state &= ~(1UL << pin);
    }
    shift_flush_locked();
    portEXIT_CRITICAL(&s_shift_mux);
}

static void stepper_enable_all(bool enable)
{
    shift_write(RT_I2S_X_DISABLE_PIN, !enable);
}

static uint32_t pulse_interval_us(int rpm)
{
    if (rpm <= 0) {
        rpm = 100;
    }
    return 60000000UL / ((uint32_t)RT_MOTOR_STEPS * RT_FULL_STEP * (uint32_t)rpm);
}

esp_err_t rt_stepper_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = BIT64(RT_I2S_OUT_BCK) | BIT64(RT_I2S_OUT_WS) | BIT64(RT_I2S_OUT_DATA),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "shift-register gpio config failed");
    gpio_set_level(RT_I2S_OUT_BCK, 0);
    gpio_set_level(RT_I2S_OUT_WS, 0);
    gpio_set_level(RT_I2S_OUT_DATA, 0);
    s_shift_state = BIT(RT_I2S_X_DISABLE_PIN) | BIT(RT_I2S_X_DIRECTION_PIN) | BIT(RT_I2S_Y_DIRECTION_PIN);
    portENTER_CRITICAL(&s_shift_mux);
    shift_flush_locked();
    portEXIT_CRITICAL(&s_shift_mux);
    stepper_enable_all(false);
    return ESP_OK;
}

void rt_stepper_set_speed(int stepper_num, int rpm)
{
    if (stepper_num < 1 || stepper_num > 2) {
        return;
    }
    s_steppers[stepper_num].rpm = rpm > 0 ? rpm : 100;
}

esp_err_t rt_stepper_step(int stepper_num, long steps, bool reverse)
{
    if (stepper_num < 1 || stepper_num > 2 || steps == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (reverse) {
        steps = -steps;
    }

    const rt_stepper_t *motor = &s_steppers[stepper_num];
    bool forward = steps >= 0;
    long pulse_count = (labs(steps) * RT_MOTOR_STEPS * RT_FULL_STEP) / 360L;
    if (pulse_count <= 0) {
        return ESP_OK;
    }

    uint32_t interval = pulse_interval_us(motor->rpm);
    if (interval < 2) {
        interval = 2;
    }

    stepper_enable_all(true);
    shift_write(motor->dir, forward);
    esp_rom_delay_us(2);

    for (long i = 0; i < pulse_count; i++) {
        int64_t start = esp_timer_get_time();
        shift_write(motor->step, true);
        esp_rom_delay_us(2);
        shift_write(motor->step, false);
        while ((esp_timer_get_time() - start) < interval) {
            if ((interval - (esp_timer_get_time() - start)) > 50) {
                taskYIELD();
            }
        }
    }

    stepper_enable_all(false);
    return ESP_OK;
}

esp_err_t rt_stepper_beep(unsigned long time_ms)
{
    shift_write(RT_I2S_BEEPER_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(time_ms));
    shift_write(RT_I2S_BEEPER_PIN, false);
    return ESP_OK;
}
