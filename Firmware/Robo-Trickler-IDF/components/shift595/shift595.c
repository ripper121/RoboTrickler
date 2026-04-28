#include "shift595.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "pinConfig.h"

static const char *TAG = "shift595";

#define SHIFT595_CLOCK_DELAY_US 1U

static SemaphoreHandle_t s_lock;
static uint32_t s_state;
static bool s_initialized;

static esp_err_t shift595_validate_bit(uint8_t bit)
{
    ESP_RETURN_ON_FALSE(bit < PINCFG_SHIFT_NUM_BITS, ESP_ERR_INVALID_ARG, TAG, "bit out of range");
    return ESP_OK;
}

static void shift595_clock_bit(bool level)
{
    gpio_set_level(PINCFG_SHIFT_DATA_GPIO, level ? 1 : 0);
    esp_rom_delay_us(SHIFT595_CLOCK_DELAY_US);
    gpio_set_level(PINCFG_SHIFT_BCK_GPIO, 1);
    esp_rom_delay_us(SHIFT595_CLOCK_DELAY_US);
    gpio_set_level(PINCFG_SHIFT_BCK_GPIO, 0);
}

static void shift595_latch(void)
{
    esp_rom_delay_us(SHIFT595_CLOCK_DELAY_US);
    gpio_set_level(PINCFG_SHIFT_WS_GPIO, 1);
    esp_rom_delay_us(SHIFT595_CLOCK_DELAY_US);
    gpio_set_level(PINCFG_SHIFT_WS_GPIO, 0);
}

static void shift595_write_locked(uint32_t state)
{
    for (int bit = PINCFG_SHIFT_NUM_BITS - 1; bit >= 0; bit--) {
        shift595_clock_bit((state & BIT(bit)) != 0U);
    }
    shift595_latch();
}

esp_err_t shift595_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_config_t io_config = {
        .pin_bit_mask = BIT64(PINCFG_SHIFT_BCK_GPIO) |
                        BIT64(PINCFG_SHIFT_WS_GPIO) |
                        BIT64(PINCFG_SHIFT_DATA_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock != NULL, ESP_ERR_NO_MEM, TAG, "lock allocation failed");

    ESP_RETURN_ON_ERROR(gpio_config(&io_config), TAG, "gpio config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PINCFG_SHIFT_BCK_GPIO, 0), TAG, "bck low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PINCFG_SHIFT_WS_GPIO, 0), TAG, "ws low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PINCFG_SHIFT_DATA_GPIO, 0), TAG, "data low failed");

    s_state = BIT(PINCFG_SHIFT_X_ENABLE_BIT);
    shift595_write_locked(s_state);
    s_initialized = true;
    ESP_LOGI(TAG, "shift-register output initialized");
    return ESP_OK;
}

esp_err_t shift595_write(uint32_t state)
{
    ESP_RETURN_ON_ERROR(shift595_init(), TAG, "init failed");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state = state;
    shift595_write_locked(s_state);
    xSemaphoreGive(s_lock);

    return ESP_OK;
}

uint32_t shift595_get_state(void)
{
    uint32_t state = 0;

    if (s_lock == NULL) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    state = s_state;
    xSemaphoreGive(s_lock);
    return state;
}

esp_err_t shift595_set_bit(uint8_t bit, bool level)
{
    ESP_RETURN_ON_ERROR(shift595_validate_bit(bit), TAG, "invalid bit");
    ESP_RETURN_ON_ERROR(shift595_init(), TAG, "init failed");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (level) {
        s_state |= BIT(bit);
    } else {
        s_state &= ~BIT(bit);
    }
    shift595_write_locked(s_state);
    xSemaphoreGive(s_lock);

    return ESP_OK;
}

esp_err_t shift595_pulse_bit(uint8_t bit, uint32_t high_us, uint32_t low_us)
{
    ESP_RETURN_ON_ERROR(shift595_validate_bit(bit), TAG, "invalid bit");
    ESP_RETURN_ON_ERROR(shift595_init(), TAG, "init failed");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state |= BIT(bit);
    shift595_write_locked(s_state);
    esp_rom_delay_us(high_us);
    s_state &= ~BIT(bit);
    shift595_write_locked(s_state);
    if (low_us > 0U) {
        esp_rom_delay_us(low_us);
    }
    xSemaphoreGive(s_lock);

    return ESP_OK;
}
