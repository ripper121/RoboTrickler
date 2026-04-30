#include "display.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "board.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"

static const char *TAG = "rt_display";
static SemaphoreHandle_t s_lvgl_mutex;
static lv_display_t *s_display;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static bool s_message_open;
static bool s_confirm_open;
static bool s_confirm_result;
static lv_obj_t *s_button_no;
static lv_obj_t *s_label_no;
static char s_log_lines[14][96];

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    lv_display_flush_ready((lv_display_t *)user_ctx);
    return false;
}

static uint16_t clamp_u16(int32_t value, uint16_t max)
{
    if (value < 0) {
        return 0;
    }
    if (value > max) {
        return max;
    }
    return (uint16_t)value;
}

static uint16_t touch_to_raw_x(uint16_t x)
{
    return x <= RT_LCD_H_RES ? (uint16_t)((uint32_t)x * 4095U / RT_LCD_H_RES) : x;
}

static uint16_t touch_to_raw_y(uint16_t y)
{
    return y <= RT_LCD_V_RES ? (uint16_t)((uint32_t)y * 4095U / RT_LCD_V_RES) : y;
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
    uint8_t count = *point_num < max_point_num ? *point_num : max_point_num;
    for (uint8_t i = 0; i < count; i++) {
        uint16_t raw_x = touch_to_raw_x(x[i]);
        uint16_t raw_y = touch_to_raw_y(y[i]);
        int32_t screen_x;
        int32_t screen_y;

        if (RT_TOUCH_CAL_FLAGS & 0x01) {
            screen_x = ((int32_t)raw_y - RT_TOUCH_CAL_X0) * RT_LCD_H_RES / RT_TOUCH_CAL_X1;
            screen_y = ((int32_t)raw_x - RT_TOUCH_CAL_Y0) * RT_LCD_V_RES / RT_TOUCH_CAL_Y1;
        } else {
            screen_x = ((int32_t)raw_x - RT_TOUCH_CAL_X0) * RT_LCD_H_RES / RT_TOUCH_CAL_X1;
            screen_y = ((int32_t)raw_y - RT_TOUCH_CAL_Y0) * RT_LCD_V_RES / RT_TOUCH_CAL_Y1;
        }
        if (RT_TOUCH_CAL_FLAGS & 0x02) {
            screen_x = RT_LCD_H_RES - screen_x;
        }
        if (RT_TOUCH_CAL_FLAGS & 0x04) {
            screen_y = RT_LCD_V_RES - screen_y;
        }

        x[i] = clamp_u16(screen_x, RT_LCD_H_RES - 1);
        y[i] = clamp_u16(screen_y, RT_LCD_V_RES - 1);
    }
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    lv_draw_sw_rgb565_swap(px_map, (uint32_t)w * (uint32_t)h);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map));
}

static void touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    uint8_t count = 0;
    esp_lcd_touch_point_data_t point[1] = {0};
    esp_lcd_touch_read_data(s_touch);
    ESP_ERROR_CHECK(esp_lcd_touch_get_data(s_touch, point, &count, 1));
    if (count > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point[0].x;
        data->point.y = point[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        if (rt_lvgl_lock(500)) {
            uint32_t delay_ms = lv_timer_handler();
            rt_lvgl_unlock();
            if (delay_ms < 5) {
                delay_ms = 5;
            } else if (delay_ms > 100) {
                delay_ms = 100;
            }
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

bool rt_lvgl_lock(uint32_t timeout_ms)
{
    if (!s_lvgl_mutex) {
        return true;
    }
    return xSemaphoreTakeRecursive(s_lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void rt_lvgl_unlock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreGiveRecursive(s_lvgl_mutex);
    }
}

esp_err_t rt_display_init(void)
{
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mutex, ESP_ERR_NO_MEM, TAG, "lvgl mutex alloc failed");

    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << RT_LCD_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl), TAG, "backlight gpio failed");
    gpio_set_level(RT_LCD_BACKLIGHT, RT_LCD_BACKLIGHT_OFF_LEVEL);

    spi_bus_config_t buscfg = {
        .sclk_io_num = RT_LCD_SCK,
        .mosi_io_num = RT_LCD_MOSI,
        .miso_io_num = RT_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = RT_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(RT_LCD_HOST, &buscfg, RT_LCD_DMA_CH);
    ESP_RETURN_ON_FALSE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, ret, TAG, "lcd spi init failed");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = RT_LCD_DC,
        .cs_gpio_num = RT_LCD_CS,
        .pclk_hz = RT_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)RT_LCD_HOST, &io_cfg, &io), TAG, "lcd io failed");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = RT_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(rt_esp_lcd_new_panel_st7796(io, &panel_cfg, &s_panel), TAG, "st7796 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, RT_LCD_SWAP_XY), TAG, "panel swap xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, RT_LCD_MIRROR_X, RT_LCD_MIRROR_Y), TAG, "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "panel on failed");
    gpio_set_level(RT_LCD_BACKLIGHT, RT_LCD_BACKLIGHT_ON_LEVEL);

    lv_init();
    s_display = lv_display_create(RT_LCD_H_RES, RT_LCD_V_RES);
    size_t draw_sz = RT_LCD_H_RES * RT_LCD_DRAW_BUF_ROWS * sizeof(lv_color16_t);
    void *buf1 = spi_bus_dma_memory_alloc(RT_LCD_HOST, draw_sz, 0);
    void *buf2 = spi_bus_dma_memory_alloc(RT_LCD_HOST, draw_sz, 0);
    ESP_RETURN_ON_FALSE(buf1 && buf2, ESP_ERR_NO_MEM, TAG, "lvgl draw buffer alloc failed");
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_display, buf1, buf2, draw_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(io, &cbs, s_display), TAG, "flush callback failed");

    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_spi_config_t touch_io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(RT_TOUCH_CS);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)RT_LCD_HOST, &touch_io_cfg, &touch_io), TAG, "touch io failed");
    esp_lcd_touch_config_t touch_cfg = {
        .x_max = RT_LCD_H_RES,
        .y_max = RT_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .process_coordinates = touch_process_coordinates,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_spi_xpt2046(touch_io, &touch_cfg, &s_touch), TAG, "xpt2046 failed");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, s_display);
    lv_indev_set_read_cb(indev, touch_cb);

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick), TAG, "lvgl tick timer failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick, 2000), TAG, "lvgl tick start failed");

    if (rt_lvgl_lock(1000)) {
        ui_init();
        lv_obj_t *content = lv_tabview_get_content(ui_TabView);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(content, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
        rt_lvgl_unlock();
    }
    BaseType_t task_ret = xTaskCreatePinnedToCore(lvgl_task,
                                                  "lvgl",
                                                  RT_LVGL_TASK_STACK_BYTES,
                                                  NULL,
                                                  RT_LVGL_TASK_PRIORITY,
                                                  NULL,
                                                  RT_LVGL_TASK_CORE);
    ESP_RETURN_ON_FALSE(task_ret == pdPASS, ESP_ERR_NO_MEM, TAG, "lvgl task create failed");
    return ESP_OK;
}

void rt_ui_set_label(lv_obj_t *label, const char *text)
{
    if (label && rt_lvgl_lock(1000)) {
        lv_label_set_text(label, text ? text : "");
        rt_lvgl_unlock();
    }
}

void rt_ui_set_label_color(lv_obj_t *label, uint32_t color)
{
    if (label && rt_lvgl_lock(1000)) {
        lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
        rt_lvgl_unlock();
    }
}

void rt_ui_set_obj_bg(lv_obj_t *obj, uint32_t color)
{
    if (obj && rt_lvgl_lock(1000)) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
        rt_lvgl_unlock();
    }
}

void rt_ui_set_weight(float weight, int decimals, const char *unit)
{
    if (decimals < 0) {
        decimals = 0;
    } else if (decimals > 6) {
        decimals = 6;
    }
    char text[32];
    snprintf(text, sizeof(text), "%.*f%s", decimals, weight, unit ? unit : "");
    rt_ui_set_label(ui_LabelTricklerWeight, text);
}

void rt_ui_log(const char *text, bool no_history)
{
    rt_ui_set_label(ui_LabelInfo, text);
    if (no_history) {
        return;
    }
    for (size_t i = 0; i + 1 < sizeof(s_log_lines) / sizeof(s_log_lines[0]); i++) {
        strlcpy(s_log_lines[i], s_log_lines[i + 1], sizeof(s_log_lines[i]));
    }
    strlcpy(s_log_lines[13], text ? text : "", sizeof(s_log_lines[13]));
    char combined[1400] = {0};
    for (size_t i = 0; i < sizeof(s_log_lines) / sizeof(s_log_lines[0]); i++) {
        if (s_log_lines[i][0]) {
            strlcat(combined, s_log_lines[i], sizeof(combined));
            strlcat(combined, "\n", sizeof(combined));
        }
    }
    rt_ui_set_label(ui_LabelLog, combined);
}

static void confirm_no_cb(lv_event_t *e)
{
    (void)e;
    s_confirm_result = false;
    s_message_open = false;
    s_confirm_open = false;
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
}

void rt_ui_message(const char *message, const lv_font_t *font, lv_color_t color, bool wait)
{
    if (rt_lvgl_lock(1000)) {
        if (s_button_no) {
            lv_obj_add_flag(s_button_no, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_x(ui_ButtonMessage, 0);
        lv_label_set_text(ui_LabelButtonMessage, "OK");
        lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
        lv_label_set_text(ui_LabelMessages, message ? message : "");
        lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
        s_message_open = true;
        s_confirm_open = false;
        rt_lvgl_unlock();
    }
    while (wait && s_message_open) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool rt_ui_confirm(const char *message, const lv_font_t *font, lv_color_t color)
{
    s_confirm_result = false;
    if (rt_lvgl_lock(1000)) {
        if (!s_button_no) {
            s_button_no = lv_btn_create(ui_PanelMessages);
            lv_obj_set_size(s_button_no, 100, 50);
            lv_obj_set_x(s_button_no, 70);
            lv_obj_set_y(s_button_no, 45);
            lv_obj_set_align(s_button_no, LV_ALIGN_CENTER);
            s_label_no = lv_label_create(s_button_no);
            lv_obj_set_align(s_label_no, LV_ALIGN_CENTER);
            lv_label_set_text(s_label_no, "No");
            lv_obj_add_event_cb(s_button_no, confirm_no_cb, LV_EVENT_CLICKED, NULL);
        }
        lv_obj_set_x(ui_ButtonMessage, -70);
        lv_label_set_text(ui_LabelButtonMessage, "Yes");
        lv_obj_clear_flag(s_button_no, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
        lv_label_set_text(ui_LabelMessages, message ? message : "");
        lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
        s_message_open = true;
        s_confirm_open = true;
        rt_lvgl_unlock();
    }
    while (s_message_open) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (rt_lvgl_lock(1000)) {
        if (s_button_no) {
            lv_obj_add_flag(s_button_no, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_x(ui_ButtonMessage, 0);
        lv_label_set_text(ui_LabelButtonMessage, "OK");
        rt_lvgl_unlock();
    }
    return s_confirm_result;
}

void rt_ui_apply_static_text(const char *start_text, const char *ok_text)
{
    if (rt_lvgl_lock(1000)) {
        lv_label_set_text(ui_LabelTricklerStart, start_text ? start_text : "Start");
        lv_label_set_text(ui_LabelButtonMessage, ok_text ? ok_text : "OK");
        rt_lvgl_unlock();
    }
}

void message_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_confirm_open) {
        s_confirm_result = true;
    }
    s_message_open = false;
    s_confirm_open = false;
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
}
