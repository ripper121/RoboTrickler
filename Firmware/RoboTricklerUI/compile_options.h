#pragma once

// Central compile-time feature switches. Use 1 to enable, 0 to disable.
#define DEBUG 0
#define ENABLE_LITTLEFS 1 //turn off for legacy devices with less than 8MB flash memory
#define ENABLE_SCREENSHOT 0

// Select the display connected to the MKS DLC32 display connector. Both panels
// use the same SPI and touch pins; the controller and usable LVGL viewport differ.
#define DISPLAY_MODEL_TS35 35
#define DISPLAY_MODEL_TS24 24
#define DISPLAY_MODEL DISPLAY_MODEL_TS24
