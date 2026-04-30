#include "esp_lcd_st7796.h"

#include <stdlib.h>
#include <sys/cdefs.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ST7796_CMD_COLMOD 0x3A
#define ST7796_CMD_INVCTR 0xB4
#define ST7796_CMD_DFUNCTR 0xB6
#define ST7796_CMD_CSCON 0xF0
#define ST7796_CMD_DOCA 0xE8
#define ST7796_CMD_PWCTRL2 0xC1
#define ST7796_CMD_PWCTRL3 0xC2
#define ST7796_CMD_VMCTRL 0xC5
#define ST7796_CMD_GMCTRP1 0xE0
#define ST7796_CMD_GMCTRN1 0xE1

static const char *TAG = "lcd.st7796";

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    gpio_num_t reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
} st7796_panel_t;

static esp_err_t tx_param(esp_lcd_panel_io_handle_t io, int cmd, const uint8_t *data, size_t len)
{
    return esp_lcd_panel_io_tx_param(io, cmd, data, len);
}

static esp_err_t panel_st7796_del(esp_lcd_panel_t *panel)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    if (st->reset_gpio_num >= 0) {
        gpio_reset_pin(st->reset_gpio_num);
    }
    free(st);
    return ESP_OK;
}

static esp_err_t panel_st7796_reset(esp_lcd_panel_t *panel)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    if (st->reset_gpio_num >= 0) {
        gpio_set_level(st->reset_gpio_num, st->reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(st->reset_gpio_num, !st->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(tx_param(st->io, LCD_CMD_SWRESET, NULL, 0), TAG, "software reset failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t panel_st7796_init(esp_lcd_panel_t *panel)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    esp_lcd_panel_io_handle_t io = st->io;

    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "reset failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "sleep out failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_CSCON, (uint8_t[]){0xC3}, 1), TAG, "cmd set 1 failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_CSCON, (uint8_t[]){0x96}, 1), TAG, "cmd set 2 failed");
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_MADCTL, &st->madctl_val, 1), TAG, "madctl failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_COLMOD, &st->colmod_val, 1), TAG, "colmod failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_INVCTR, (uint8_t[]){0x01}, 1), TAG, "invctr failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_DFUNCTR, (uint8_t[]){0x80, 0x02, 0x3B}, 3), TAG, "dfunctr failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_DOCA, (uint8_t[]){0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8), TAG, "doca failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_PWCTRL2, (uint8_t[]){0x06}, 1), TAG, "pwctrl2 failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_PWCTRL3, (uint8_t[]){0xA7}, 1), TAG, "pwctrl3 failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_VMCTRL, (uint8_t[]){0x18}, 1), TAG, "vmctrl failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_GMCTRP1, (uint8_t[]){0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B}, 14), TAG, "gamma+ failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_GMCTRN1, (uint8_t[]){0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B}, 14), TAG, "gamma- failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_CSCON, (uint8_t[]){0x3C}, 1), TAG, "cmd unset 1 failed");
    ESP_RETURN_ON_ERROR(tx_param(io, ST7796_CMD_CSCON, (uint8_t[]){0x69}, 1), TAG, "cmd unset 2 failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return tx_param(io, LCD_CMD_DISPON, NULL, 0);
}

static esp_err_t panel_st7796_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    x_start += st->x_gap;
    x_end += st->x_gap;
    y_start += st->y_gap;
    y_end += st->y_gap;
    ESP_RETURN_ON_ERROR(tx_param(st->io, LCD_CMD_CASET, (uint8_t[]){(x_start >> 8) & 0xff, x_start & 0xff, ((x_end - 1) >> 8) & 0xff, (x_end - 1) & 0xff}, 4), TAG, "caset failed");
    ESP_RETURN_ON_ERROR(tx_param(st->io, LCD_CMD_RASET, (uint8_t[]){(y_start >> 8) & 0xff, y_start & 0xff, ((y_end - 1) >> 8) & 0xff, (y_end - 1) & 0xff}, 4), TAG, "raset failed");
    size_t len = (size_t)(x_end - x_start) * (size_t)(y_end - y_start) * st->fb_bits_per_pixel / 8;
    return esp_lcd_panel_io_tx_color(st->io, LCD_CMD_RAMWR, color_data, len);
}

static esp_err_t panel_st7796_invert_color(esp_lcd_panel_t *panel, bool invert)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    return tx_param(st->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
}

static esp_err_t panel_st7796_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    if (mirror_x) {
        st->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        st->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        st->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        st->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    return tx_param(st->io, LCD_CMD_MADCTL, &st->madctl_val, 1);
}

static esp_err_t panel_st7796_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    if (swap_axes) {
        st->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        st->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    return tx_param(st->io, LCD_CMD_MADCTL, &st->madctl_val, 1);
}

static esp_err_t panel_st7796_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    st->x_gap = x_gap;
    st->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_st7796_disp_on_off(esp_lcd_panel_t *panel, bool on)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    return tx_param(st->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
}

static esp_err_t panel_st7796_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    st7796_panel_t *st = __containerof(panel, st7796_panel_t, base);
    ESP_RETURN_ON_ERROR(tx_param(st->io, sleep ? LCD_CMD_SLPIN : LCD_CMD_SLPOUT, NULL, 0), TAG, "sleep command failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

esp_err_t rt_esp_lcd_new_panel_st7796(const esp_lcd_panel_io_handle_t io,
                                      const esp_lcd_panel_dev_config_t *panel_dev_config,
                                      esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    st7796_panel_t *st = calloc(1, sizeof(*st));
    ESP_RETURN_ON_FALSE(st, ESP_ERR_NO_MEM, TAG, "no memory");
    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t rst_conf = {
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_conf), err, TAG, "reset gpio config failed");
    }
    st->io = io;
    st->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st->reset_level = panel_dev_config->flags.reset_active_high;
    st->fb_bits_per_pixel = 16;
    st->colmod_val = 0x55;
    st->madctl_val = panel_dev_config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR ? LCD_CMD_BGR_BIT : 0;
    st->base.del = panel_st7796_del;
    st->base.reset = panel_st7796_reset;
    st->base.init = panel_st7796_init;
    st->base.draw_bitmap = panel_st7796_draw_bitmap;
    st->base.invert_color = panel_st7796_invert_color;
    st->base.mirror = panel_st7796_mirror;
    st->base.swap_xy = panel_st7796_swap_xy;
    st->base.set_gap = panel_st7796_set_gap;
    st->base.disp_on_off = panel_st7796_disp_on_off;
    st->base.disp_sleep = panel_st7796_sleep;
    *ret_panel = &st->base;
    return ESP_OK;
err:
    free(st);
    return ret;
}
