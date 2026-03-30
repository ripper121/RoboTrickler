#pragma once

#include <stdint.h>
#include "driver/gpio.h"

// ============================================================
// Scale serial interface — UART1 wrapper for RS232 scale
// ============================================================
void serial1_begin(int baud, gpio_num_t rx_pin, gpio_num_t tx_pin);
int  serial1_available(void);
int  serial1_read_until(uint8_t terminator, char *buf, int max_len);
void serial1_write(const uint8_t *data, size_t len);
void serial1_write_str(const char *s);
void serial1_flush(void);
