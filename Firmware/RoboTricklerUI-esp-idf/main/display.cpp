#include "display.h"
#include "pindef.h"

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display";

#define LCD_SPI_HOST         SPI3_HOST
#define LCD_SPI_FREQ_HZ      40000000
#define LCD_TOUCH_FREQ_HZ    2000000
#define LCD_CMD_BITS         8
#define LCD_PARAM_BITS       8
#define LCD_DRAW_BUF_PIXELS  (LV_HOR_RES_MAX * LV_VER_RES_MAX / 10)
#define LCD_BL_ON_LEVEL      0
#define LCD_BL_OFF_LEVEL     1
#define TOUCH_CAL_X0         248U
#define TOUCH_CAL_X1         3571U
#define TOUCH_CAL_Y0         202U
#define TOUCH_CAL_Y1         3647U
#define TOUCH_CAL_FLAGS      5U

static esp_lcd_panel_io_handle_t s_lcd_io = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static lv_disp_drv_t *s_pending_flush_drv = NULL;

static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t s_lv_buf[LCD_DRAW_BUF_PIXELS];

static uint16_t clamp_touch_coord(int32_t value, uint16_t max_value)
{
    if (value < 0) {
        return 0;
    }
    if (value >= (int32_t)max_value) {
        return (uint16_t)(max_value - 1U);
    }
    return (uint16_t)value;
}

static void swap_rgb565_bytes(lv_color_t *color_data, size_t pixel_count)
{
    uint8_t *bytes = (uint8_t *)color_data;

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t offset = i * sizeof(lv_color_t);
        const uint8_t tmp = bytes[offset];
        bytes[offset] = bytes[offset + 1];
        bytes[offset + 1] = tmp;
    }
}

static void touch_process_coordinates(esp_lcd_touch_handle_t tp,
                                      uint16_t *x,
                                      uint16_t *y,
                                      uint16_t *strength,
                                      uint8_t *point_num,
                                      uint8_t max_point_num)
{
    (void)tp;
    (void)strength;
    (void)max_point_num;

    if ((x == NULL) || (y == NULL) || (point_num == NULL)) {
        return;
    }

    for (uint8_t i = 0; i < *point_num; ++i) {
        const uint16_t raw_x = x[i];
        const uint16_t raw_y = y[i];
        int32_t screen_x = 0;
        int32_t screen_y = 0;

        if ((TOUCH_CAL_FLAGS & 0x01U) != 0U) {
            screen_x = ((int32_t)raw_y - (int32_t)TOUCH_CAL_X0) * LV_HOR_RES_MAX / (int32_t)TOUCH_CAL_X1;
            screen_y = ((int32_t)raw_x - (int32_t)TOUCH_CAL_Y0) * LV_VER_RES_MAX / (int32_t)TOUCH_CAL_Y1;
        } else {
            screen_x = ((int32_t)raw_x - (int32_t)TOUCH_CAL_X0) * LV_HOR_RES_MAX / (int32_t)TOUCH_CAL_X1;
            screen_y = ((int32_t)raw_y - (int32_t)TOUCH_CAL_Y0) * LV_VER_RES_MAX / (int32_t)TOUCH_CAL_Y1;
        }

        if ((TOUCH_CAL_FLAGS & 0x02U) != 0U) {
            screen_x = LV_HOR_RES_MAX - screen_x;
        }
        if ((TOUCH_CAL_FLAGS & 0x04U) != 0U) {
            screen_y = LV_VER_RES_MAX - screen_y;
        }

        x[i] = clamp_touch_coord(screen_x, LV_HOR_RES_MAX);
        y[i] = clamp_touch_coord(screen_y, LV_VER_RES_MAX);
    }
}

static void lcd_send_param(uint8_t command, const void *data, size_t data_size)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_lcd_io, command, data, data_size));
}

static void st7796_init(void)
{
    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    lcd_send_param(0x01, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_send_param(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_send_param(0xF0, (uint8_t[]) {0xC3}, 1);
    lcd_send_param(0xF0, (uint8_t[]) {0x96}, 1);
    // Match TFT_eSPI ST7796 setRotation(0): MX | MY | MV | BGR
    lcd_send_param(0x36, (uint8_t[]) {0xE8}, 1);
    lcd_send_param(0x3A, (uint8_t[]) {0x05}, 1);
    lcd_send_param(0xB4, (uint8_t[]) {0x01}, 1);
    lcd_send_param(0xB7, (uint8_t[]) {0xC6}, 1);
    lcd_send_param(0xE8, (uint8_t[]) {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8);
    lcd_send_param(0xC1, (uint8_t[]) {0x06}, 1);
    lcd_send_param(0xC2, (uint8_t[]) {0xA7}, 1);
    lcd_send_param(0xC5, (uint8_t[]) {0x18}, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_send_param(0xE0, (uint8_t[]) {
        0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B
    }, 14);
    lcd_send_param(0xE1, (uint8_t[]) {
        0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B
    }, 14);

    lcd_send_param(0xF0, (uint8_t[]) {0x3C}, 1);
    lcd_send_param(0xF0, (uint8_t[]) {0x69}, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_send_param(0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(25));
}

static bool lcd_flush_done_cb(esp_lcd_panel_io_handle_t panel_io,
                              esp_lcd_panel_io_event_data_t *edata,
                              void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;

    if (s_pending_flush_drv != NULL) {
        lv_disp_flush_ready(s_pending_flush_drv);
        s_pending_flush_drv = NULL;
    }
    return false;
}

extern "C" void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    const uint8_t column_data[] = {
        (uint8_t)(area->x1 >> 8),
        (uint8_t)(area->x1 & 0xFF),
        (uint8_t)(area->x2 >> 8),
        (uint8_t)(area->x2 & 0xFF),
    };
    const uint8_t row_data[] = {
        (uint8_t)(area->y1 >> 8),
        (uint8_t)(area->y1 & 0xFF),
        (uint8_t)(area->y2 >> 8),
        (uint8_t)(area->y2 & 0xFF),
    };
    const size_t pixel_count = (size_t)(area->x2 - area->x1 + 1) * (size_t)(area->y2 - area->y1 + 1);

    // Match the old TFT_eSPI pushColors(..., true) path without changing LVGL's global color config.
    swap_rgb565_bytes(color_p, pixel_count);

    s_pending_flush_drv = drv;
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_lcd_io, 0x2A, column_data, sizeof(column_data)));
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_lcd_io, 0x2B, row_data, sizeof(row_data)));
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(s_lcd_io, 0x2C, color_p, pixel_count * sizeof(lv_color_t)));
}

extern "C" void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    esp_lcd_touch_point_data_t touch_points[1] = {};
    uint8_t touch_count = 0;

    esp_lcd_touch_read_data(s_touch);
    if (esp_lcd_touch_get_data(s_touch, touch_points, &touch_count, 1) == ESP_OK && touch_count > 0) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = (lv_coord_t)touch_points[0].x;
        data->point.y = (lv_coord_t)touch_points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

extern "C" void display_init(void)
{
    gpio_config_t io_config = {};
    io_config.pin_bit_mask = (1ULL << LCD_RST) | (1ULL << LCD_BL);
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pull_up_en = GPIO_PULLUP_DISABLE;
    io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_config));

    gpio_set_level(LCD_BL, LCD_BL_OFF_LEVEL);

    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = LCD_MOSI;
    bus_config.miso_io_num = LCD_MISO;
    bus_config.sclk_io_num = LCD_SCK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = LCD_DRAW_BUF_PIXELS * sizeof(lv_color_t) + 16;
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t lcd_io = NULL;
    esp_lcd_panel_io_spi_config_t lcd_io_config = {};
    lcd_io_config.cs_gpio_num = LCD_CS;
    lcd_io_config.dc_gpio_num = LCD_DC;
    lcd_io_config.spi_mode = 0;
    lcd_io_config.pclk_hz = LCD_SPI_FREQ_HZ;
    lcd_io_config.trans_queue_depth = 10;
    lcd_io_config.lcd_cmd_bits = LCD_CMD_BITS;
    lcd_io_config.lcd_param_bits = LCD_PARAM_BITS;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &lcd_io_config, &lcd_io));
    s_lcd_io = lcd_io;

    const esp_lcd_panel_io_callbacks_t lcd_callbacks = {
        .on_color_trans_done = lcd_flush_done_cb,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(s_lcd_io, &lcd_callbacks, NULL));

    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_spi_config_t touch_io_config = {};
    touch_io_config.cs_gpio_num = TOUCH_CS;
    touch_io_config.dc_gpio_num = GPIO_NUM_NC;
    touch_io_config.spi_mode = 0;
    touch_io_config.pclk_hz = ESP_LCD_TOUCH_SPI_CLOCK_HZ;
    touch_io_config.trans_queue_depth = 3;
    touch_io_config.lcd_cmd_bits = 8;
    touch_io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &touch_io_config, &touch_io));

    esp_lcd_touch_config_t touch_config = {};
    touch_config.x_max = LV_HOR_RES_MAX;
    touch_config.y_max = LV_VER_RES_MAX;
    touch_config.rst_gpio_num = GPIO_NUM_NC;
    touch_config.int_gpio_num = GPIO_NUM_NC;
    touch_config.flags.swap_xy = 0;
    touch_config.flags.mirror_x = 0;
    touch_config.flags.mirror_y = 0;
    touch_config.process_coordinates = touch_process_coordinates;
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(touch_io, &touch_config, &s_touch));

    st7796_init();
    gpio_set_level(LCD_BL, LCD_BL_ON_LEVEL);

    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_lv_buf, NULL, LCD_DRAW_BUF_PIXELS);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Display init OK (%dx%d)", LV_HOR_RES_MAX, LV_VER_RES_MAX);
}
