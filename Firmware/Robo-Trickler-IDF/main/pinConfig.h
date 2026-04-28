#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Central board mapping for the RoboTricklerUI ESP32 hardware.
 * Source: RoboTricklerUI/pindef.h.
 */

/* ST7796 TFT + XPT2046 touch, VSPI/SPI3 */
#define PINCFG_LCD_SCLK_GPIO            GPIO_NUM_18
#define PINCFG_LCD_MISO_GPIO            GPIO_NUM_19
#define PINCFG_LCD_MOSI_GPIO            GPIO_NUM_23
#define PINCFG_LCD_DC_GPIO              GPIO_NUM_33
#define PINCFG_LCD_BACKLIGHT_GPIO       GPIO_NUM_5
#define PINCFG_LCD_BACKLIGHT_ON_LEVEL   0
#define PINCFG_LCD_RST_GPIO             GPIO_NUM_27
#define PINCFG_LCD_CS_GPIO              GPIO_NUM_25
#define PINCFG_TOUCH_CS_GPIO            GPIO_NUM_26
#define PINCFG_LCD_H_RES                480
#define PINCFG_LCD_V_RES                320
#define PINCFG_LCD_SPI_FREQ_HZ          40000000
#define PINCFG_TOUCH_SPI_FREQ_HZ        2000000

/* SPI SD card, HSPI/SPI2 */
#define PINCFG_SD_SCK_GPIO              GPIO_NUM_14
#define PINCFG_SD_MISO_GPIO             GPIO_NUM_12
#define PINCFG_SD_MOSI_GPIO             GPIO_NUM_13
#define PINCFG_SD_CS_GPIO               GPIO_NUM_15
#define PINCFG_SD_DET_GPIO              GPIO_NUM_39
#define PINCFG_SD_SPI_FREQ_KHZ          4000

/* RS232 scale UART. Arduino Serial1.begin() used RX=IIC_SCL, TX=IIC_SDA. */
#define PINCFG_RS232_RX_GPIO            GPIO_NUM_4
#define PINCFG_RS232_TX_GPIO            GPIO_NUM_0

/* Shift-register output bus formerly driven through the Arduino I2S output layer. */
#define PINCFG_SHIFT_BCK_GPIO           GPIO_NUM_16
#define PINCFG_SHIFT_WS_GPIO            GPIO_NUM_17
#define PINCFG_SHIFT_DATA_GPIO          GPIO_NUM_21
#define PINCFG_SHIFT_NUM_BITS           32

#define PINCFG_SHIFT_X_ENABLE_BIT       0
#define PINCFG_SHIFT_X_STEP_BIT         1
#define PINCFG_SHIFT_X_DIR_BIT          2
#define PINCFG_SHIFT_Y_ENABLE_BIT       0
#define PINCFG_SHIFT_Y_STEP_BIT         5
#define PINCFG_SHIFT_Y_DIR_BIT          6
#define PINCFG_SHIFT_BUZZER_BIT         7

#ifdef __cplusplus
}
#endif
