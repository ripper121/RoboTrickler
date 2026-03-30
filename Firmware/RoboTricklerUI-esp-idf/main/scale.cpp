#include "scale.h"

#include <string.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "scale";

#define SCALE_UART      UART_NUM_1
#define SCALE_BUF_SIZE  256

void serial1_begin(int baud, gpio_num_t rx_pin, gpio_num_t tx_pin) {
    uart_config_t cfg = {};
    cfg.baud_rate  = baud;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_APB;
    ESP_ERROR_CHECK(uart_driver_install(SCALE_UART, SCALE_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(SCALE_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SCALE_UART, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

int serial1_available(void) {
    size_t len = 0;
    uart_get_buffered_data_len(SCALE_UART, &len);
    return (int)len;
}

// Read bytes until terminator or buffer full; returns byte count (excluding terminator)
int serial1_read_until(uint8_t terminator, char *buf, int max_len) {
    int idx = 0;
    while (idx < max_len - 1) {
        uint8_t c;
        int r = uart_read_bytes(SCALE_UART, &c, 1, pdMS_TO_TICKS(10));
        if (r <= 0) break;
        if (c == terminator) break;
        buf[idx++] = (char)c;
    }
    buf[idx] = '\0';
    return idx;
}

void serial1_write(const uint8_t *data, size_t len) {
    uart_write_bytes(SCALE_UART, (const char *)data, len);
}

void serial1_write_str(const char *s) {
    uart_write_bytes(SCALE_UART, s, strlen(s));
}

void serial1_flush(void) {
    uart_flush(SCALE_UART);
    uart_flush_input(SCALE_UART);
}
