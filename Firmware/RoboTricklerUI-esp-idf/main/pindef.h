#pragma once
#include "driver/gpio.h"

// Shift register (bit-bang) — stepper control
// NOTE: Despite the "I2S" prefix in names, these are plain GPIO used with
// a software shift register (ShiftRegister74HC595). No I2S peripheral is used.
#define SR_DATA_PIN     GPIO_NUM_21   // I2S_OUT_DATA
#define SR_CLK_PIN      GPIO_NUM_16   // I2S_OUT_BCK
#define SR_LATCH_PIN    GPIO_NUM_17   // I2S_OUT_WS

// Stepper motor bit positions inside the shift register byte
#define I2S_X_DISABLE_PIN   0
#define I2S_X_STEP_PIN      1
#define I2S_X_DIRECTION_PIN 2

#define I2S_Y_STEP_PIN      5
#define I2S_Y_DIRECTION_PIN 6
#define I2S_Y_DISABLE_PIN   0   // shared with X disable

#define I2S_Z_STEP_PIN      3
#define I2S_Z_DIRECTION_PIN 4
#define I2S_Z_DISABLE_PIN   0

// Beeper is also on the shift register
#define I2S_BEEPER_PIN      7

// Spindle
#define SPINDLE_OUTPUT_PIN  GPIO_NUM_32

// Limit switches
#define X_LIMIT_PIN         GPIO_NUM_36
#define Y_LIMIT_PIN         GPIO_NUM_35
#define Z_LIMIT_PIN         GPIO_NUM_34

// LCD (ST7796) — VSPI / SPI3
#define LCD_SCK     GPIO_NUM_18
#define LCD_MISO    GPIO_NUM_19
#define LCD_MOSI    GPIO_NUM_23
#define LCD_DC      GPIO_NUM_33   // Data/Command (RS)
#define LCD_BL      GPIO_NUM_5    // Backlight enable (active LOW)
#define LCD_RST     GPIO_NUM_27
#define LCD_CS      GPIO_NUM_25

// Touch (XPT2046) — same SPI bus
#define TOUCH_CS    GPIO_NUM_26

// I2C (not used for main function, kept for reference)
#define IIC_SCL     GPIO_NUM_4
#define IIC_SDA     GPIO_NUM_0

// SD card — HSPI / SPI2
#define SD_SCK      GPIO_NUM_14
#define SD_MISO     GPIO_NUM_12
#define SD_MOSI     GPIO_NUM_13
#define SD_SS       GPIO_NUM_15
#define SD_DET_PIN  GPIO_NUM_39

// SD SPI frequency
#define SD_SPI_FREQ_HZ  40000000

// SD card mount point
#define SD_MOUNT_POINT  "/sdcard"
