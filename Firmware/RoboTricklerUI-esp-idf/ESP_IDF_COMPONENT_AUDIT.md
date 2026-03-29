# ESP-IDF 6.0 Component Audit

This project currently builds, but it still carries several Arduino-derived dependencies and a few large custom subsystems that ESP-IDF already covers better.

The goal of this audit is:

- align module structure and style with local ESP-IDF 6.0 examples in `C:\esp\v6.0\esp-idf\examples`
- replace Arduino libraries with ESP-IDF components or managed components
- reduce custom hardware glue where ESP-IDF already provides a standard driver layer

## Current Arduino-derived dependencies

The following components still pull source from `../../../Arduino-Libs`:

- `components/lvgl/CMakeLists.txt`
- `components/ArduinoJson/CMakeLists.txt`
- `components/QuickPID/CMakeLists.txt`

The main app also still relies on Arduino compatibility wrappers in many files:

- `main/main.cpp`
- `main/display.cpp`
- `main/sd_config.cpp`
- `main/webserver.cpp`
- `main/update.cpp`
- `main/stepper_driver.cpp`

## Replacement Map

### 1. Display + Touch

Current state:

- `main/display.cpp` implements its own SPI LCD transaction layer
- `main/display.cpp` hardcodes the ST7796 init sequence
- `main/display.cpp` hardcodes XPT2046 SPI reads and touch calibration
- `components/lvgl` vendors LVGL from `Arduino-Libs`

Recommended replacement:

- use `esp_lcd` for panel I/O and panel abstraction
- use a controller-specific panel driver component for ST7796 instead of the handwritten init sequence
- use `atanisoft/esp_lcd_touch_xpt2046` for touch handling
- use managed `lvgl/lvgl` instead of the Arduino copy

Local ESP-IDF references:

- `C:\esp\v6.0\esp-idf\examples\peripherals\lcd\spi_lcd_touch\main\spi_lcd_touch_example_main.c`
- `C:\esp\v6.0\esp-idf\examples\peripherals\lcd\spi_lcd_touch\main\idf_component.yml`

Why:

- this matches current ESP-IDF LCD driver structure
- it removes most of `display.cpp` low-level SPI command code
- it gives a standard touch API instead of raw XPT2046 transactions

Notes:

- ESP-IDF core provides `esp_lcd`, but ST7796 and XPT2046 integration are expected through componentized drivers
- the example shows the exact pattern to follow even though it uses `ST7789`

### 2. SD Card Mounting

Current state:

- `main/sd_config.cpp` mounts SDSPI manually and mixes mount logic with config parsing

Recommended replacement:

- keep `esp_vfs_fat` + `sdspi_host`, but refactor the mount code to follow the file-serving example layout
- split storage mounting into a dedicated `storage` or `sdcard` module with `esp_err_t`-returning init helpers

Local ESP-IDF reference:

- `C:\esp\v6.0\esp-idf\examples\protocols\http_server\file_serving\main\mount.c`

Why:

- the example uses the standard `esp_vfs_fat_sdspi_mount` flow, error handling, and card reporting
- current code is functionally similar, but not organized in an ESP-IDF example style

### 3. JSON Config Parsing

Current state:

- `main/sd_config.cpp` uses ArduinoJson
- `components/ArduinoJson/CMakeLists.txt` points to `../../../Arduino-Libs/ArduinoJson`

Recommended replacement:

- replace ArduinoJson with managed `espressif/cjson`

Local ESP-IDF references:

- `C:\esp\v6.0\esp-idf\examples\peripherals\i2c\i2c_slave_network_sensor\main\idf_component.yml`
- `C:\esp\v6.0\esp-idf\examples\peripherals\i2c\i2c_slave_network_sensor\main\i2c_slave_main.c`

Why:

- ESP-IDF examples already use `espressif/cjson`
- this removes the Arduino dependency cleanly

Notes:

- `cJSON` is not a bundled core component under `components/`
- it is still the example-backed ESP-IDF ecosystem choice here

### 4. Wi-Fi Connection Flow

Current state:

- `main/webserver.cpp` implements its own Wi-Fi event flow directly in the file

Recommended replacement:

- refactor Wi-Fi startup and teardown to mirror `protocol_examples_common`
- separate networking bootstrap from HTTP route registration

Local ESP-IDF reference:

- `C:\esp\v6.0\esp-idf\examples\common_components\protocol_examples_common\wifi_connect.c`

Why:

- current code works, but the example structure is more idiomatic
- it uses cleaner registration and unregister flow for event handlers
- it keeps connection lifecycle separate from application logic

### 5. HTTP File Server

Current state:

- `main/webserver.cpp` implements a custom SD-backed file server, directory list, create, delete, and download logic

Recommended replacement:

- continue using `esp_http_server`
- reshape the module around example-style request handlers and a clearer server context object
- reuse the file-serving example's path handling and storage abstraction patterns

Local ESP-IDF reference:

- `C:\esp\v6.0\esp-idf\examples\protocols\http_server\file_serving`

Why:

- this area already uses the correct core component
- the improvement needed here is style and structure, not a different server library

### 6. OTA Update

Current state:

- `main/update.cpp` performs OTA from `/update.bin` on SD using `esp_ota_*`

Recommended replacement:

- keep the ESP-IDF OTA APIs
- refactor to example-style `esp_err_t` helpers and explicit validation/logging boundaries

Why:

- this subsystem is already using the correct ESP-IDF primitives
- no Arduino library replacement is needed here

### 7. Stepper Motor Control

Current state:

- `main/stepper_driver.cpp` implements blocking pulse generation with busy-wait timing
- it depends on Arduino-like timing helpers from `arduino_compat.h`
- motion timing and GPIO toggling are handwritten

Recommended replacement:

- replace the custom pulse generator with the ESP-IDF RMT stepper pattern
- use GPIO only for direction and enable
- use RMT encoders for accel, cruise, and decel phases

Local ESP-IDF reference:

- `C:\esp\v6.0\esp-idf\examples\peripherals\rmt\stepper_motor\main\stepper_motor_example_main.c`

Why:

- this removes timing-critical busy loops from the CPU
- it follows a first-party example built for stepper pulse generation
- it is a much better fit for precise stepping under FreeRTOS

### 8. PID Control

Current state:

- `main/main.cpp` uses `QuickPID`
- `components/QuickPID/CMakeLists.txt` points to `../../../Arduino-Libs/QuickPID/src`

Recommended replacement:

- remove QuickPID
- replace it with a small local ESP-IDF-style PID module in this repo

Why:

- ESP-IDF 6.0 does not provide a standard first-party application PID component for this use case
- carrying an Arduino PID library is not consistent with the project direction
- a small local module is likely simpler than adapting another framework-specific dependency

Notes:

- this is the one area where the best replacement is probably not another external component
- the code should expose plain C or C++ functions and use normal project types, not Arduino abstractions

## Recommended dependency end state

After the migration, the project should move toward this split:

- core ESP-IDF components:
  - `esp_lcd`
  - `driver`
  - `esp_http_server`
  - `esp_wifi`
  - `esp_event`
  - `esp_netif`
  - `esp_vfs_fat`
  - `sdmmc`
  - `app_update`
  - `esp_timer`
  - `mdns`
  - `freertos`
- managed components:
  - `lvgl/lvgl`
  - `espressif/cjson`
  - `atanisoft/esp_lcd_touch_xpt2046`
  - a panel driver component for `ST7796` if one is used instead of a local panel wrapper
- local project modules:
  - PID controller
  - application logic
  - UI screens and event glue

## Style changes needed

To match ESP-IDF examples more closely, the app code should trend toward:

- smaller modules with one responsibility
- `esp_err_t` return values for init and I/O operations
- `ESP_ERROR_CHECK()` at integration boundaries
- `static const char *TAG = "...";` per translation unit
- explicit init and deinit paths
- less global state mixed into driver code
- less Arduino-like naming and helper behavior

Examples from the current codebase that should be cleaned up during migration:

- `serial1_*` helpers in `main/main.cpp` should become a dedicated UART-backed scale module
- `display.cpp` should stop owning raw panel command sequences directly
- `webserver.cpp` should stop mixing Wi-Fi setup, filesystem helpers, and HTTP handlers in one file
- `sd_config.cpp` should stop mixing mount logic, file utilities, and JSON parsing

## Migration order

Recommended order:

1. Replace `ArduinoJson` with `espressif/cjson`
2. Replace `QuickPID` with a local PID module
3. Replace the custom stepper driver with the RMT-based implementation
4. Replace vendored LVGL and custom touch handling with managed LVGL plus `esp_lcd` and `esp_lcd_touch_xpt2046`
5. Refactor storage, Wi-Fi, and HTTP modules to example-style structure
6. Remove `arduino_compat.h` once timing and utility call sites are gone

## Immediate cleanup targets in this repo

Files most worth touching first:

- `main/sd_config.cpp`
- `main/main.cpp`
- `main/stepper_driver.cpp`
- `main/display.cpp`
- `components/ArduinoJson/CMakeLists.txt`
- `components/QuickPID/CMakeLists.txt`
- `components/lvgl/CMakeLists.txt`
- `main/CMakeLists.txt`
- `main/idf_component.yml`

## Bottom line

The biggest mismatches with an ESP-IDF-native project are:

- vendored Arduino libraries for LVGL, JSON, and PID
- handwritten LCD/touch and stepper driver layers where ESP-IDF already has better component boundaries
- application modules that combine too many responsibilities compared with ESP-IDF examples

The clean target architecture is:

- ESP-IDF core drivers for hardware access
- managed components for LVGL, touch, and JSON
- a small local PID module
- application code reorganized to mirror the patterns used in local ESP-IDF examples
