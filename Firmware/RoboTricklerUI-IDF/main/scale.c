#include "scale.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "driver/uart.h"
#include "esp_check.h"

#define RT_SCALE_UART UART_NUM_1
#define RT_SCALE_RX_BUF 256

static const char *TAG = "rt_scale";

static bool contains_ignore_case(const char *text, const char *needle)
{
    if (!text || !needle || needle[0] == '\0') {
        return false;
    }
    for (; *text; text++) {
        const char *h = text;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (*n == '\0') {
            return true;
        }
    }
    return false;
}

static void parse_weight(const char *input, float *value, int *decimal_places)
{
    char filtered[64];
    int j = 0;
    bool dot_found = false;
    int decimals = 0;
    for (int i = 0; input[i] != '\0' && j < (int)sizeof(filtered) - 1; i++) {
        if (isdigit((unsigned char)input[i]) || input[i] == '.' || input[i] == ',') {
            if (input[i] == '.' || input[i] == ',') {
                if (dot_found) {
                    break;
                }
                dot_found = true;
                filtered[j++] = '.';
            } else {
                if (dot_found) {
                    decimals++;
                }
                filtered[j++] = input[i];
            }
        }
    }
    filtered[j] = '\0';
    *decimal_places = decimals;
    *value = strtof(filtered, NULL);
    if (strchr(input, '-') != NULL) {
        *value *= -1.0f;
    }
}

static void request_weight(const rt_config_t *config)
{
    if (strcmp(config->scale_protocol, "GG") == 0) {
        const uint8_t cmd[] = {0x1B, 0x70, 0x0D, 0x0A};
        uart_write_bytes(RT_SCALE_UART, cmd, sizeof(cmd));
    } else if (strcmp(config->scale_protocol, "AD") == 0) {
        uart_write_bytes(RT_SCALE_UART, "Q\r\n", 3);
    } else if (strcmp(config->scale_protocol, "KERN") == 0) {
        uart_write_bytes(RT_SCALE_UART, "w", 1);
    } else if (strcmp(config->scale_protocol, "KERN-ABT") == 0) {
        uart_write_bytes(RT_SCALE_UART, "D05\r\n", 5);
    } else if (strcmp(config->scale_protocol, "SBI") == 0) {
        uart_write_bytes(RT_SCALE_UART, "P\r\n", 3);
    } else if (strcmp(config->scale_protocol, "CUSTOM") == 0) {
        uart_write_bytes(RT_SCALE_UART, config->scale_custom_code, strlen(config->scale_custom_code));
    }
}

esp_err_t rt_scale_init(int baud)
{
    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(RT_SCALE_UART, RT_SCALE_RX_BUF, 0, 0, NULL, 0), TAG, "uart install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(RT_SCALE_UART, &uart_config), TAG, "uart config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(RT_SCALE_UART, RT_SCALE_TX_PIN, RT_SCALE_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "uart pins failed");
    return ESP_OK;
}

esp_err_t rt_scale_poll(const rt_config_t *config, rt_scale_reading_t *reading, uint32_t timeout_ms)
{
    request_weight(config);
    uint8_t buf[64];
    int len = uart_read_bytes(RT_SCALE_UART, buf, sizeof(buf) - 1, pdMS_TO_TICKS(timeout_ms));
    if (len <= 0) {
        reading->has_new_weight = false;
        return ESP_ERR_TIMEOUT;
    }
    buf[len] = '\0';
    if (strchr((const char *)buf, '.') == NULL && strchr((const char *)buf, ',') == NULL) {
        reading->has_new_weight = false;
        return ESP_ERR_INVALID_RESPONSE;
    }
    parse_weight((const char *)buf, &reading->weight, &reading->decimal_places);
    strlcpy(reading->unit, contains_ignore_case((const char *)buf, "gn") || contains_ignore_case((const char *)buf, "gr") ? " gn" : " g", sizeof(reading->unit));
    reading->has_new_weight = true;
    return ESP_OK;
}
