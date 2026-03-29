#pragma once
#include <stdint.h>

// ============================================================
// Bit-bang 74HC595 shift register driver
// Replaces ShiftRegister74HC595<1> from the Arduino project.
// Pins: DATA=GPIO21, CLK=GPIO16, LATCH=GPIO17
// ============================================================

#ifdef __cplusplus
extern "C" {
#endif

// Initialise GPIO pins for the shift register
void sr_init(void);

// Set a single bit and immediately push all 8 bits to the register
void sr_set(uint8_t bit_index, uint8_t value);

// Set all bits LOW
void sr_set_all_low(void);

// Set all bits HIGH
void sr_set_all_high(void);

// Push current shadow register to hardware without changing any bit
void sr_update(void);

#ifdef __cplusplus
}
#endif
