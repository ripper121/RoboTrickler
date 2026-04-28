#include "display.h"

#include <sys/param.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pinConfig.h"

static const char *TAG = "display";

#define DISPLAY_LCD_SPI_HOST SPI3_HOST
#define DISPLAY_LCD_SPI_DMA_CH SPI_DMA_CH2

#define DISPLAY_LVGL_DRAW_BUF_LINES     20U
#define DISPLAY_LVGL_TICK_PERIOD_MS     2U
#define DISPLAY_LVGL_TASK_STACK_SIZE    4096U
#define DISPLAY_LVGL_TASK_PRIORITY      2U
#define DISPLAY_LVGL_TASK_MIN_DELAY_MS  5U
#define DISPLAY_LVGL_TASK_MAX_DELAY_MS  50U
#define DISPLAY_TOUCH_CAL_X0            248
#define DISPLAY_TOUCH_CAL_X1            3571
#define DISPLAY_TOUCH_CAL_Y0            202
#define DISPLAY_TOUCH_CAL_Y1            3647
#define DISPLAY_TOUCH_CAL_ROTATE        true
#define DISPLAY_TOUCH_CAL_INVERT_X      false
#define DISPLAY_TOUCH_CAL_INVERT_Y      true
#define DISPLAY_TOUCH_Z_THRESHOLD       20U
#define DISPLAY_TOUCH_RAW_STABLE_DELTA  20U
#define DISPLAY_TOUCH_READ_ATTEMPTS     5U
#define DISPLAY_TOUCH_SETTLE_MAX_READS  8U
#define DISPLAY_TOUCH_Z_DELAY_US        1000U
#define DISPLAY_TOUCH_XY_DELAY_US       2000U

static SemaphoreHandle_t s_lvgl_mutex;
static esp_lcd_panel_handle_t s_panel;
static spi_device_handle_t s_touch_dev;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;
static bool s_initialized;

static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

static esp_err_t touch_spi_transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t len)
{
    spi_transaction_t transaction = {
        .length = len * 8U,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    if (s_touch_dev == NULL || tx_data == NULL || rx_data == NULL || len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    return spi_device_polling_transmit(s_touch_dev, &transaction);
}

static uint16_t touch_get_raw_z(void)
{
    WORD_ALIGNED_ATTR const uint8_t tx_data[5] = {0xB0, 0xC0, 0x00, 0x00, 0x00};
    WORD_ALIGNED_ATTR uint8_t rx_data[5] = {0};

    if (touch_spi_transfer(tx_data, rx_data, sizeof(tx_data)) != ESP_OK) {
        return 0;
    }

    const int32_t z1 = (int32_t)((((uint16_t)rx_data[1] << 8) | rx_data[2]) >> 3);
    const int32_t z2 = (int32_t)((((uint16_t)rx_data[3] << 8) | rx_data[4]) >> 3);
    int32_t z = 0x0FFF + z1 - z2;

    if (z == 4095) {
        return 0;
    }
    if (z < 0) {
        return 0;
    }
    if (z > 4095) {
        return 4095;
    }
    return (uint16_t)z;
}

static bool touch_get_raw_xy(uint16_t *raw_x, uint16_t *raw_y)
{
    WORD_ALIGNED_ATTR const uint8_t tx_data[17] = {
        0xD0, 0x00, 0xD0, 0x00, 0xD0, 0x00, 0xD0, 0x00, 0x90,
        0x00, 0x90, 0x00, 0x90, 0x00, 0x90, 0x00, 0x00,
    };
    WORD_ALIGNED_ATTR uint8_t rx_data[17] = {0};

    if (raw_x == NULL || raw_y == NULL ||
        touch_spi_transfer(tx_data, rx_data, sizeof(tx_data)) != ESP_OK) {
        return false;
    }

    *raw_x = ((uint16_t)rx_data[7] << 5) | ((rx_data[8] >> 3) & 0x1FU);
    *raw_y = ((uint16_t)rx_data[15] << 5) | ((rx_data[16] >> 3) & 0x1FU);
    return true;
}

static bool touch_valid_raw(uint16_t *raw_x, uint16_t *raw_y)
{
    uint16_t x1 = 0;
    uint16_t y1 = 0;
    uint16_t x2 = 0;
    uint16_t y2 = 0;
    uint16_t z1 = 1;
    uint16_t z2 = 0;
    bool pressure_settled = false;

    if (raw_x == NULL || raw_y == NULL) {
        return false;
    }

    for (uint8_t i = 0; i < DISPLAY_TOUCH_SETTLE_MAX_READS; i++) {
        z2 = z1;
        z1 = touch_get_raw_z();
        esp_rom_delay_us(DISPLAY_TOUCH_Z_DELAY_US);

        if (z1 <= z2) {
            pressure_settled = true;
            break;
        }
    }

    if (!pressure_settled || z1 <= DISPLAY_TOUCH_Z_THRESHOLD) {
        return false;
    }

    if (!touch_get_raw_xy(&x1, &y1)) {
        return false;
    }

    esp_rom_delay_us(DISPLAY_TOUCH_Z_DELAY_US);
    if (touch_get_raw_z() <= DISPLAY_TOUCH_Z_THRESHOLD) {
        return false;
    }

    esp_rom_delay_us(DISPLAY_TOUCH_XY_DELAY_US);
    if (!touch_get_raw_xy(&x2, &y2)) {
        return false;
    }

    if (abs_diff_u16(x1, x2) > DISPLAY_TOUCH_RAW_STABLE_DELTA ||
        abs_diff_u16(y1, y2) > DISPLAY_TOUCH_RAW_STABLE_DELTA) {
        return false;
    }

    *raw_x = x1;
    *raw_y = y1;
    return true;
}

static bool touch_convert_raw_xy(uint16_t raw_x, uint16_t raw_y, uint16_t *screen_x, uint16_t *screen_y)
{
    int32_t x = 0;
    int32_t y = 0;

    if (screen_x == NULL || screen_y == NULL) {
        return false;
    }

    if (DISPLAY_TOUCH_CAL_ROTATE) {
        x = (((int32_t)raw_y - DISPLAY_TOUCH_CAL_X0) * PINCFG_LCD_H_RES) / DISPLAY_TOUCH_CAL_X1;
        y = (((int32_t)raw_x - DISPLAY_TOUCH_CAL_Y0) * PINCFG_LCD_V_RES) / DISPLAY_TOUCH_CAL_Y1;
    } else {
        x = (((int32_t)raw_x - DISPLAY_TOUCH_CAL_X0) * PINCFG_LCD_H_RES) / DISPLAY_TOUCH_CAL_X1;
        y = (((int32_t)raw_y - DISPLAY_TOUCH_CAL_Y0) * PINCFG_LCD_V_RES) / DISPLAY_TOUCH_CAL_Y1;
    }

    if (DISPLAY_TOUCH_CAL_INVERT_X) {
        x = PINCFG_LCD_H_RES - x;
    }
    if (DISPLAY_TOUCH_CAL_INVERT_Y) {
        y = PINCFG_LCD_V_RES - y;
    }

    if (x < 0 || x >= PINCFG_LCD_H_RES || y < 0 || y >= PINCFG_LCD_V_RES) {
        return false;
    }

    *screen_x = (uint16_t)x;
    *screen_y = (uint16_t)y;
    return true;
}

static bool display_get_touch(uint16_t *screen_x, uint16_t *screen_y)
{
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    bool valid = false;

    if (screen_x == NULL || screen_y == NULL || s_touch_dev == NULL) {
        return false;
    }

    for (uint8_t i = 0; i < DISPLAY_TOUCH_READ_ATTEMPTS; i++) {
        if (touch_valid_raw(&raw_x, &raw_y)) {
            valid = true;
        }
    }

    return valid && touch_convert_raw_xy(raw_x, raw_y, screen_x, screen_y);
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(DISPLAY_LVGL_TICK_PERIOD_MS);
}

static void swap_rgb565_bytes(lv_color_t *colors, uint32_t count)
{
    uint16_t *data = (uint16_t *)colors;

    for (uint32_t i = 0; i < count; i++) {
        data[i] = (uint16_t)((data[i] << 8) | (data[i] >> 8));
    }
}

static bool lcd_flush_done_cb(esp_lcd_panel_io_handle_t panel_io,
                              esp_lcd_panel_io_event_data_t *edata,
                              void *user_ctx)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;

    (void)panel_io;
    (void)edata;

    lv_disp_flush_ready(disp_drv);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map)
{
    const int x1 = area->x1;
    const int x2 = area->x2;
    const int y1 = area->y1;
    const int y2 = area->y2;
    const uint32_t pixel_count = (uint32_t)(x2 - x1 + 1) * (uint32_t)(y2 - y1 + 1);

    (void)disp_drv;

    swap_rgb565_bytes(color_map, pixel_count);
    esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2 + 1, y2 + 1, color_map);
}

static void lvgl_touch_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touch_x = 0;
    uint16_t touch_y = 0;

    (void)indev_drv;

    if (data == NULL) {
        return;
    }

    if (s_touch_dev == NULL) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    if (display_get_touch(&touch_x, &touch_y)) {
        data->point.x = touch_x;
        data->point.y = touch_y;
        data->state = LV_INDEV_STATE_PR;
        return;
    }

    data->state = LV_INDEV_STATE_REL;
}

static void lvgl_task(void *arg)
{
    (void)arg;

    while (true) {
        uint32_t delay_ms = DISPLAY_LVGL_TASK_MAX_DELAY_MS;

        if (display_lock(DISPLAY_LVGL_TASK_MAX_DELAY_MS)) {
            delay_ms = lv_timer_handler();
            display_unlock();
        }

        delay_ms = MAX(delay_ms, DISPLAY_LVGL_TASK_MIN_DELAY_MS);
        delay_ms = MIN(delay_ms, DISPLAY_LVGL_TASK_MAX_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

bool display_lock(uint32_t timeout_ms)
{
    if (s_lvgl_mutex == NULL) {
        return false;
    }

    return xSemaphoreTakeRecursive(s_lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void display_unlock(void)
{
    if (s_lvgl_mutex != NULL) {
        xSemaphoreGiveRecursive(s_lvgl_mutex);
    }
}

static esp_err_t display_init_backlight(void)
{
    gpio_config_t backlight_config = {
        .pin_bit_mask = BIT64(PINCFG_LCD_BACKLIGHT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&backlight_config), TAG, "backlight gpio config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PINCFG_LCD_BACKLIGHT_GPIO, !PINCFG_LCD_BACKLIGHT_ON_LEVEL),
                        TAG, "backlight off failed");
    return ESP_OK;
}

static esp_err_t display_init_spi_bus(void)
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = PINCFG_LCD_MOSI_GPIO,
        .miso_io_num = PINCFG_LCD_MISO_GPIO,
        .sclk_io_num = PINCFG_LCD_SCLK_GPIO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = PINCFG_LCD_H_RES * DISPLAY_LVGL_DRAW_BUF_LINES * sizeof(lv_color_t),
    };

    esp_err_t ret = spi_bus_initialize(DISPLAY_LCD_SPI_HOST, &bus_config, DISPLAY_LCD_SPI_DMA_CH);
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }

    return ret;
}

static esp_err_t display_init_panel(esp_lcd_panel_io_handle_t *out_io)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PINCFG_LCD_DC_GPIO,
        .cs_gpio_num = PINCFG_LCD_CS_GPIO,
        .pclk_hz = PINCFG_LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = lcd_flush_done_cb,
        .user_ctx = &s_disp_drv,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PINCFG_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(DISPLAY_LCD_SPI_HOST, &io_config, &io),
                        TAG, "new panel io failed");
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(io, &panel_config, &s_panel),
                        TAG, "new ST7796 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), TAG, "panel swap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, true), TAG, "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "panel display on failed");

    *out_io = io;
    return ESP_OK;
}

static esp_err_t display_init_touch(void)
{
    spi_device_interface_config_t touch_config = {
        .clock_speed_hz = PINCFG_TOUCH_SPI_FREQ_HZ,
        .mode = 0,
        .spics_io_num = PINCFG_TOUCH_CS_GPIO,
        .queue_size = 1,
    };

    ESP_RETURN_ON_ERROR(spi_bus_add_device(DISPLAY_LCD_SPI_HOST, &touch_config, &s_touch_dev),
                        TAG, "add XPT2046 SPI device failed");
    return ESP_OK;
}

static esp_err_t display_init_lvgl(void)
{
    const size_t draw_buf_pixels = PINCFG_LCD_H_RES * DISPLAY_LVGL_DRAW_BUF_LINES;
    const size_t draw_buf_size = draw_buf_pixels * sizeof(lv_color_t);

    lv_init();

    s_buf1 = heap_caps_malloc(draw_buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf2 = heap_caps_malloc(draw_buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(s_buf1 != NULL && s_buf2 != NULL, ESP_ERR_NO_MEM, TAG, "LVGL draw buffer allocation failed");

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, draw_buf_pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = PINCFG_LCD_H_RES;
    s_disp_drv.ver_res = PINCFG_LCD_V_RES;
    s_disp_drv.flush_cb = lvgl_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&s_indev_drv);

    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_timer_args, &tick_timer), TAG, "lvgl tick timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, DISPLAY_LVGL_TICK_PERIOD_MS * 1000U),
                        TAG, "lvgl tick timer start failed");

    ESP_RETURN_ON_FALSE(xTaskCreate(lvgl_task,
                                    "lvgl",
                                    DISPLAY_LVGL_TASK_STACK_SIZE,
                                    NULL,
                                    DISPLAY_LVGL_TASK_PRIORITY,
                                    NULL) == pdPASS,
                        ESP_ERR_NO_MEM,
                        TAG,
                        "LVGL task create failed");
    return ESP_OK;
}

esp_err_t display_init(void)
{
    esp_lcd_panel_io_handle_t lcd_io = NULL;

    if (s_initialized) {
        return ESP_OK;
    }

    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mutex != NULL, ESP_ERR_NO_MEM, TAG, "LVGL mutex allocation failed");

    ESP_RETURN_ON_ERROR(display_init_backlight(), TAG, "backlight init failed");
    ESP_RETURN_ON_ERROR(display_init_spi_bus(), TAG, "LCD SPI bus init failed");
    ESP_RETURN_ON_ERROR(display_init_panel(&lcd_io), TAG, "LCD panel init failed");
    (void)lcd_io;
    ESP_RETURN_ON_ERROR(display_init_touch(), TAG, "touch init failed");
    ESP_RETURN_ON_ERROR(display_init_lvgl(), TAG, "LVGL init failed");

    ESP_RETURN_ON_ERROR(gpio_set_level(PINCFG_LCD_BACKLIGHT_GPIO, PINCFG_LCD_BACKLIGHT_ON_LEVEL),
                        TAG, "backlight on failed");
    s_initialized = true;
    ESP_LOGI(TAG, "ST7796 LVGL display initialized (%ux%u)", PINCFG_LCD_H_RES, PINCFG_LCD_V_RES);
    return ESP_OK;
}
