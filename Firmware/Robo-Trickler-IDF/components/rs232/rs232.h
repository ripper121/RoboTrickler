#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "pinConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RS232_DEFAULT_UART_PORT      UART_NUM_2
#define RS232_DEFAULT_TX_GPIO        PINCFG_RS232_TX_GPIO
#define RS232_DEFAULT_RX_GPIO        PINCFG_RS232_RX_GPIO
#define RS232_DEFAULT_RX_BUFFER_SIZE 256U

typedef struct rs232_t *rs232_handle_t;

typedef struct {
    uart_port_t uart_port;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    int baud_rate;
    int timeout_ms;
    size_t rx_buffer_size;
} rs232_config_t;

esp_err_t rs232_init(const rs232_config_t *config, rs232_handle_t *out_handle);
esp_err_t rs232_delete(rs232_handle_t handle);

esp_err_t rs232_send_byte(rs232_handle_t handle, uint8_t value);
esp_err_t rs232_send_string(rs232_handle_t handle, const char *text);
esp_err_t rs232_read_byte(rs232_handle_t handle, uint8_t *out_value);
esp_err_t rs232_read_until(rs232_handle_t handle,
                           uint8_t terminator,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *out_len);

#ifdef __cplusplus
}
#endif
