#include "scale.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "rs232.h"

static const char *TAG = "scale";

struct scale_t {
    rs232_handle_t rs232;
    int request_delay_ms;
    char separator;
    uint8_t request_bytes[SCALE_COMMAND_MAX_LEN];
    size_t request_len;
    uint8_t tare_bytes[SCALE_COMMAND_MAX_LEN];
    size_t tare_len;
    uint8_t terminator;
};

static esp_err_t scale_validate_handle(scale_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->rs232 != NULL, ESP_ERR_INVALID_STATE, TAG, "rs232 is null");
    return ESP_OK;
}

static int scale_hex_nibble(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    c = (char)toupper((unsigned char)c);
    if ((c >= 'A') && (c <= 'F')) {
        return 10 + (c - 'A');
    }
    return -1;
}

static esp_err_t scale_parse_hex_bytes(const char *text, uint8_t *out_bytes, size_t out_capacity, size_t *out_len)
{
    size_t count = 0;

    ESP_RETURN_ON_FALSE(out_len != NULL, ESP_ERR_INVALID_ARG, TAG, "out_len is null");
    *out_len = 0;

    if ((text == NULL) || (text[0] == '\0')) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(out_bytes != NULL, ESP_ERR_INVALID_ARG, TAG, "out_bytes is null");

    while (*text != '\0') {
        int hi;
        int lo;

        while ((*text != '\0') && isspace((unsigned char)*text)) {
            text++;
        }
        if (*text == '\0') {
            break;
        }

        hi = scale_hex_nibble(text[0]);
        lo = scale_hex_nibble(text[1]);
        ESP_RETURN_ON_FALSE((hi >= 0) && (lo >= 0), ESP_ERR_INVALID_ARG, TAG, "invalid hex byte");
        ESP_RETURN_ON_FALSE(count < out_capacity, ESP_ERR_INVALID_ARG, TAG, "hex command too long");

        out_bytes[count++] = (uint8_t)((hi << 4) | lo);
        text += 2;

        while ((*text != '\0') && isspace((unsigned char)*text)) {
            text++;
        }
    }

    *out_len = count;
    return ESP_OK;
}

static esp_err_t scale_send_command(scale_handle_t handle, const uint8_t *command, size_t command_len)
{
    for (size_t i = 0; i < command_len; i++) {
        ESP_RETURN_ON_ERROR(rs232_send_byte(handle->rs232, command[i]), TAG, "send command byte failed");
    }
    return ESP_OK;
}

static bool scale_is_number_char(char c, char separator)
{
    return isdigit((unsigned char)c) || (c == '+') || (c == '-') || (c == separator);
}

static esp_err_t scale_parse_float_from_response(const uint8_t *buffer, size_t len, char separator, float *out_value)
{
    char token[32];
    bool response_negative = false;

    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer is null");
    ESP_RETURN_ON_FALSE(out_value != NULL, ESP_ERR_INVALID_ARG, TAG, "out_value is null");

    response_negative = memchr(buffer, '-', len) != NULL;

    for (size_t i = 0; i < len; i++) {
        size_t token_len = 0;
        bool has_digit = false;
        bool has_separator = false;

        if (!scale_is_number_char((char)buffer[i], separator)) {
            continue;
        }

        while ((i + token_len) < len &&
               scale_is_number_char((char)buffer[i + token_len], separator) &&
               token_len < (sizeof(token) - 1U)) {
            char c = (char)buffer[i + token_len];

            if (isdigit((unsigned char)c)) {
                has_digit = true;
            } else if (c == separator) {
                if (has_separator) {
                    break;
                }
                has_separator = true;
                c = '.';
            } else if ((c == '+') || (c == '-')) {
                if (token_len != 0) {
                    break;
                }
            }

            token[token_len++] = c;
        }

        if (has_digit && token_len > 0) {
            char *end_ptr = NULL;

            token[token_len] = '\0';
            *out_value = strtof(token, &end_ptr);
            if ((end_ptr != NULL) && (*end_ptr == '\0')) {
                if (response_negative && *out_value > 0.0f) {
                    *out_value = -*out_value;
                }
                return ESP_OK;
            }
        }
    }

    return ESP_ERR_NOT_FOUND;
}

static bool scale_read_error_is_quiet(esp_err_t ret)
{
    return ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NOT_FOUND || ret == ESP_ERR_INVALID_SIZE;
}

esp_err_t scale_init(const config_scale_t *config, scale_handle_t *out_handle)
{
    esp_err_t ret;
    scale_handle_t handle = NULL;
    rs232_config_t rs232_config = {
        .uart_port = RS232_DEFAULT_UART_PORT,
        .tx_gpio = RS232_DEFAULT_TX_GPIO,
        .rx_gpio = RS232_DEFAULT_RX_GPIO,
        .rx_buffer_size = RS232_DEFAULT_RX_BUFFER_SIZE,
    };

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    handle = calloc(1, sizeof(*handle));
    ESP_GOTO_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory for scale handle");

    handle->request_delay_ms = config->request_delay_ms;
    handle->separator = (config->separator[0] != '\0') ? config->separator[0] : '.';

    ret = scale_parse_hex_bytes(config->request, handle->request_bytes, sizeof(handle->request_bytes), &handle->request_len);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "invalid request command");

    ret = scale_parse_hex_bytes(config->tare, handle->tare_bytes, sizeof(handle->tare_bytes), &handle->tare_len);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "invalid tare command");

    {
        uint8_t terminator_bytes[1];
        size_t terminator_len = 0;

        ret = scale_parse_hex_bytes(config->terminator, terminator_bytes, sizeof(terminator_bytes), &terminator_len);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "invalid terminator");
        ESP_GOTO_ON_FALSE(terminator_len == 1, ESP_ERR_INVALID_ARG, err, TAG, "terminator must be exactly one byte");
        handle->terminator = terminator_bytes[0];
    }

    rs232_config.baud_rate = config->baud;
    rs232_config.timeout_ms = config->timeout_ms;

    ret = rs232_init(&rs232_config, &handle->rs232);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "rs232_init failed");

    *out_handle = handle;
    return ESP_OK;

err:
    free(handle);
    return ret;
}

esp_err_t scale_delete(scale_handle_t handle)
{
    ESP_RETURN_ON_ERROR(scale_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_ERROR(rs232_delete(handle->rs232), TAG, "rs232_delete failed");
    free(handle);
    return ESP_OK;
}

esp_err_t scale_request(scale_handle_t handle, uint8_t *buffer, size_t buffer_size, size_t *out_len)
{
    ESP_RETURN_ON_ERROR(scale_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer is null");
    ESP_RETURN_ON_FALSE(buffer_size > 0, ESP_ERR_INVALID_ARG, TAG, "buffer_size must be > 0");
    ESP_RETURN_ON_FALSE(out_len != NULL, ESP_ERR_INVALID_ARG, TAG, "out_len is null");

    ESP_RETURN_ON_ERROR(scale_send_command(handle, handle->request_bytes, handle->request_len), TAG, "send request failed");
    if (handle->request_delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(handle->request_delay_ms));
    }

    return rs232_read_until(handle->rs232, handle->terminator, buffer, buffer_size, out_len);
}

esp_err_t scale_read_unit(scale_handle_t handle, float *out_value)
{
    uint8_t buffer[SCALE_RESPONSE_MAX_LEN];
    size_t out_len = 0;
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(scale_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(out_value != NULL, ESP_ERR_INVALID_ARG, TAG, "out_value is null");

    ret = scale_request(handle, buffer, sizeof(buffer), &out_len);
    if (ret != ESP_OK) {
        if (scale_read_error_is_quiet(ret)) {
            return ret;
        }
        ESP_RETURN_ON_ERROR(ret, TAG, "scale_request failed");
    }

    ret = scale_parse_float_from_response(buffer, out_len, handle->separator, out_value);
    if (ret != ESP_OK) {
        if (scale_read_error_is_quiet(ret)) {
            return ret;
        }
        ESP_RETURN_ON_ERROR(ret, TAG, "response does not contain a float");
    }
    return ESP_OK;
}

esp_err_t scale_read_unit_stable(scale_handle_t handle, size_t sample_count, float *out_value)
{
    float stable_value = 0.0f;
    size_t consecutive_matches = 0;

    ESP_RETURN_ON_ERROR(scale_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(sample_count > 0, ESP_ERR_INVALID_ARG, TAG, "sample_count must be > 0");
    ESP_RETURN_ON_FALSE(out_value != NULL, ESP_ERR_INVALID_ARG, TAG, "out_value is null");

    while (true) {
        float next_value = 0.0f;
        esp_err_t ret = scale_read_unit(handle, &next_value);

        if (ret != ESP_OK) {
            if (scale_read_error_is_quiet(ret)) {
                return ret;
            }
            ESP_RETURN_ON_ERROR(ret, TAG, "stable read failed");
        }

        if (consecutive_matches == 0U || next_value == stable_value) {
            stable_value = next_value;
            consecutive_matches++;
            if (consecutive_matches >= sample_count) {
                *out_value = stable_value;
                return ESP_OK;
            }
        } else {
            ESP_LOGD(TAG, "stable read mismatch, restarting streak: %.3f -> %.3f", (double)stable_value, (double)next_value);
            stable_value = next_value;
            consecutive_matches = 1U;
        }
    }
}

esp_err_t scale_tare(scale_handle_t handle)
{
    ESP_RETURN_ON_ERROR(scale_validate_handle(handle), TAG, "invalid handle");
    ESP_RETURN_ON_ERROR(scale_send_command(handle, handle->tare_bytes, handle->tare_len), TAG, "send tare failed");
    return ESP_OK;
}
