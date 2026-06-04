# Changelog Firmware 2.11

### Overview
- Added expanded RS232 scale support through the new `rs232.ino` module.
- Added Firmware 2.11 config/profile options for trickle counters, start-at-zero behavior, and selectable bulk actuator handling.
- Improved Wi-Fi and webserver reliability, including reboot support from the web UI.

### Added

- Added scale protocol support for custom hex requests, and an empty protocol for no active request command.
- Added global trickle counter support.
- Added profile-level `startAtZero`, `trickleCounter`, and selectable bulk `actuator` options under profile `general`.

### Changed

- Updated firmware update checks to use the firmware-internal default update URL.
- Updated webserver startup and client handling to track active server and registered route state.
- Changed first-throw bulk movement to use the selected profile actuator instead of scanning both steppers.
- Moved weight display, parsing, and decimal-place behavior into the RS232 code path.
- Updated config parsing and saving for the new scale, trickle counter, and profile options.

### Removed

- Removed active reverse-direction profile behavior from trickling and stepper handling.
- Removed old conversion-to-degrees wording and related config behavior.

### Dependencies
- Updated Boards-Manager esp32 - Espressif Systems version `3.1.3` to `3.3.8`
- Updated LVGL from `8.3.11` to `9.5.0`.
- Updated `lv_conf.h` for LVGL 9.5.
- Updated ArduinoJson from `7.2.1` to `7.4.3`.
- Updated bundled Arduino libraries, including TFT_eSPI changes.
