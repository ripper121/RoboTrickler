# Changelog Firmware 2.10

### Overview
- Added automatic powder profile creation from the calibration run.
- Added SD-card language support with `language` in `config.txt`.
- Removed PID and logger-related firmware and UI pieces.
- Removed microstep setting from active firmware behavior.

### Added

- Added SD-card language support with `language` in `config.txt`.
- Added English and German language JSON files for firmware messages and web UI.
- Added `language.ino` with fallback strings and runtime UI text loading.
- Added automatic powder profile creation from the calibration run.
- Added new profile format using `general`, `actuator`, and `rs232TrickleMap`.
- Added `/profiles/avg.txt` and `/profiles/calibrate.txt` as the default SD profiles.
- Added profile validation, invalid profile reporting, and `.cor.txt` renaming for corrupted profiles.
- Added `/getLanguage` web API endpoint.
- Added JSON escaping for web API and file-browser output.
- Added `lvgl_compat.c` for LVGL 9 compatibility.

### Changed

- Updated firmware version to `2.10`.
- Reworked profile storage so profiles now live under `/profiles`.
- Moved target weight handling into the selected profile.
- Profile selection now reloads the profile immediately, updates the displayed target weight, and persists the selected profile to `config.txt`.
- Reworked trickling logic around the new profile fields, including tolerance, alarm threshold, weight gap, measurement counts, reverse direction, and actuator selection.
- Added first-throw bulk movement using `unitsPerThrow`, with support for `stepper1` and `stepper2`.
- Replaced external stepper and shift-register libraries with local stepper and shift-register control code.
- Stepper handling now assumes full-step A4988-style drivers.
- Reworked beeper control through the new shift-register stepper backend.
- Updated web firmware update handling with better status and error messages.
- Updated HTTPS firmware-version check for ESP32 core 3.3.8 using `NetworkClientSecure`.
- Updated SD file serving to support `.json` content type and cleaner `.gz` handling.
- Updated web profile generator and editor pages for the new profile format and language support.
- Updated Arduino build settings for ESP32 core `3.3.8`.

### Removed

- Removed PID and logger-related firmware and UI pieces.
- Removed `csv.ino`.
- Removed microstep setting from active firmware behavior.
- Removed watchdog disable and reset code.
- Removed old sample grain and gram profile files from the SD image.
- Removed bundled `StepperDriver` and `ShiftRegister74HC595` libraries.

### Dependencies
- Updated Boards-Manager esp32 - Espressif Systems version `3.1.3` to `3.3.8`
- Updated LVGL from `8.3.11` to `9.5.0`.
- Updated `lv_conf.h` for LVGL 9.5.
- Updated ArduinoJson from `7.2.1` to `7.4.3`.
- Updated bundled Arduino libraries, including TFT_eSPI changes.
