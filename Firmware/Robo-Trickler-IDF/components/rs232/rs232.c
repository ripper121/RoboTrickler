#include "rs232.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "rs232";

struct rs232_t {
    uart_port_t uart_port;
    int timeout_ms;
};

static esp_err_t rs232_validate_handle(rs232_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

static TickType_t rs232_timeout_ticks(const rs232_handle_t handle)
{
    return pdMS_TO_TICKS(handle->timeout_ms > 0 ? handle->timeout_ms : 1000);
}

esp_err_t rs232_init(const rs232_config_t *config, rs232_handle_t *out_handle)
{
    esp_err_t ret;
    rs232_handle_t handle = NULL;
    uart_config_t uart_config = { 0 };
    size_t rx_buffer_size = 0;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(config->baud_rate > 0, ESP_ERR_INVALID_ARG, TAG, "baud_rate must be > 0");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(config->tx_gpio), ESP_ERR_INVALID_ARG, TAG, "tx gpio invalid");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(config->rx_gpio), ESP_ERR_INVALID_ARG, TAG, "rx gpio invalid");

    handle = calloc(1, sizeof(*handle));
    ESP_GOTO_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory for rs232 handle");

    handle->uart_port = config->uart_port;
    handle->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 1000;
    rx_buffer_size = config->rx_buffer_size > 0 ? config->rx_buffer_size : RS232_DEFAULT_RX_BUFFER_SIZE;

    uart_config = (uart_config_t) {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ret = uart_driver_install(handle->uart_port, (int)(rx_buffer_size * 2U), 0, 0, NULL, 0);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "uart_driver_install failed");

    ret = uart_param_config(handle->uart_port, &uart_config);
    ESP_GOTO_ON_ERROR(ret, err_driver, TAG, "uart_param_config failed");

    ret = uart_set_pin(handle->uart_port, config->tx_gpio, config->rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_GOTO_ON_ERROR(ret, err_driver, TAG, "uart_set_pin failed");

    ret = uart_flush_input(handle->uart_port);
    ESP_GOTO_ON_ERROR(ret, err_driver, TAG, "uart_flush_input failed");

    *out_handle = handle;
    return ESP_OK;

err_driver:
    uart_driver_delete(handle->uart_port);
err:
    free(handle);
    return ret;
}

esp_err_t rs232_delete(rs232_handle_t handle)
{
    ESP_RETURN_ON_ERROR(rs232_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_ERROR(uart_driver_delete(handle->uart_port), TAG, "uart_driver_delete failed");
    free(handle);
    return ESP_OK;
}

esp_err_t rs232_send_byte(rs232_handle_t handle, uint8_t value)
{
    int written;

    ESP_RETURN_ON_ERROR(rs232_validate_handle(handle), TAG, "invalid handle");

    written = uart_write_bytes(handle->uart_port, &value, 1);
    ESP_RETURN_ON_FALSE(written == 1, ESP_FAIL, TAG, "short uart write");
    ESP_RETURN_ON_ERROR(uart_wait_tx_done(handle->uart_port, rs232_timeout_ticks(handle)), TAG, "uart_wait_tx_done failed");
    return ESP_OK;
}

esp_err_t rs232_send_string(rs232_handle_t handle, const char *text)
{
    int written;
    size_t len = 0;

    ESP_RETURN_ON_ERROR(rs232_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is null");

    len = strlen(text);
    if (len == 0) {
        return ESP_OK;
    }

    written = uart_write_bytes(handle->uart_port, text, len);
    ESP_RETURN_ON_FALSE(written == (int)len, ESP_FAIL, TAG, "short uart write");
    ESP_RETURN_ON_ERROR(uart_wait_tx_done(handle->uart_port, rs232_timeout_ticks(handle)), TAG, "uart_wait_tx_done failed");
    return ESP_OK;
}

esp_err_t rs232_read_byte(rs232_handle_t handle, uint8_t *out_value)
{
    int read_len;

    ESP_RETURN_ON_ERROR(rs232_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(out_value != NULL, ESP_ERR_INVALID_ARG, TAG, "out_value is null");

    read_len = uart_read_bytes(handle->uart_port, out_value, 1, rs232_timeout_ticks(handle));
    if (read_len < 0) {
        return ESP_FAIL;
    }

    return read_len == 1 ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t rs232_read_until(rs232_handle_t handle,
                           uint8_t terminator,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *out_len)
{
    size_t len = 0;

    ESP_RETURN_ON_ERROR(rs232_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer is null");
    ESP_RETURN_ON_FALSE(buffer_size > 0, ESP_ERR_INVALID_ARG, TAG, "buffer_size must be > 0");
    ESP_RETURN_ON_FALSE(out_len != NULL, ESP_ERR_INVALID_ARG, TAG, "out_len is null");

    *out_len = 0;

    while (len < buffer_size) {
        esp_err_t ret = rs232_read_byte(handle, &buffer[len]);
        if (ret != ESP_OK) {
            return (len > 0) ? ESP_OK : ret;
        }

        len++;
        if (buffer[len - 1] == terminator) {
            *out_len = len;
            return ESP_OK;
        }
    }

    *out_len = len;
    return ESP_ERR_INVALID_SIZE;
}
