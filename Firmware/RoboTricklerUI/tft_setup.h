// Project-local TFT_eSPI setup. build_opt.h force-includes this file for every
// source file, including TFT_eSPI.cpp, so the selected display is reproducible
// from this repo instead of depending on the library's global User_Setup.h.
#ifndef ROBOTRICKLER_TFT_SETUP_H
#define ROBOTRICKLER_TFT_SETUP_H

#include "compile_options.h"

// Prevent TFT_eSPI from falling back to its machine-wide User_Setup.h. This
// makes DISPLAY_MODEL the single source of the display controller and pins.
#define USER_SETUP_LOADED

#if DISPLAY_MODEL == DISPLAY_MODEL_TS24
// TS24-R is ILI9341-compatible and requires TFT_eSPI's alternate
// initialization sequence.
#define ILI9341_2_DRIVER
// This panel requires the controller's inversion mode for normal colours.
#define TFT_INVERSION_ON
// With inversion enabled this panel uses the standard ILI9341 BGR order.
#define TFT_RGB_ORDER TFT_BGR
#elif DISPLAY_MODEL == DISPLAY_MODEL_TS35
#define ST7796_DRIVER
#else
#error "DISPLAY_MODEL must be DISPLAY_MODEL_TS24 or DISPLAY_MODEL_TS35"
#endif

// MKS DLC32 TS connector pin mapping, from the MKS DLC32 firmware.
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 25
#define TFT_DC 33
#define TFT_RST 27
#define TFT_BL 5
#if DISPLAY_MODEL == DISPLAY_MODEL_TS24
#define TFT_BACKLIGHT_ON LOW
#else
#define TFT_BACKLIGHT_ON HIGH
#endif
#define TOUCH_CS 26

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2000000

#endif
