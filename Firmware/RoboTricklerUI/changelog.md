# Changelog Firmware 2.12

### Overview
- Updated firmware version from `2.11` to `2.12`.
- Improved RS232 scale handling, especially request generation, response parsing, and A&D scale support.
- Added display polish and maintenance features, including the SD logo splash, web/display screenshots, and profile deletion.
- Updated the build/tooling flow for Arduino ESP32 core `3.3.10`.

### Added

- Added built-in RS232 request commands for `GG`, `AD`, `KERN`, `KERN-ABT`, `KERN-ABS`, and `SBI` scale protocols.
- Added stricter RS232 response parsing that reads complete numeric tokens and rejects ambiguous scale lines instead of merging unrelated digits.
- Added optional debug logging for custom RS232 transmit data in both display-safe text and hex forms.
- Added `/system/logo.bmp` splash logo support during startup.
- Added BMP screenshot capture support for the LVGL display.
- Added profile delete support with confirmation, calibrate-profile protection, and automatic profile list refresh.
- Added localized fallback/error strings for corrupt config/profile handling and profile validation errors.
- Added localized web editor/file editor strings for reboot, upload, save, delete, preview, download, refresh, and expand/collapse actions.
- Added `compile_upload.py` for compiling with Arduino IDE/arduino-cli settings and uploading firmware through the web update endpoint.
- Added `create_sd_tar.py` and bundled `SD-Files.tar` packaging support.
- Added updated `Hardware/Latest` 3D model exports.

### Changed

- Updated the firmware update URL default in firmware code and kept firmware URL handling internal to firmware.
- Updated ESP32 Arduino core references and VS Code include paths from `3.3.8` to `3.3.10`.
- Changed firmware `DEBUG` default to `0`.
- Changed startup order so SD-card errors can still initialize the UI before showing blocking messages.
- Changed display logging so `noLog` status messages update the info label without appending to the scrollback log.
- Changed done-with-count display text to use localized `status_count`.
- Improved corrupt config/profile messages by routing them through language strings.
- Simplified language loading so firmware falls back through `/lang/<language>.json` and `/lang/en.json`.
- Updated default SD config values to blank Wi-Fi credentials, `GG` scale protocol, and done-only beeper behavior.
- Updated profile generator/editor pages to localize units, section names, actuator labels, and profile-step labels.
- Updated web reboot prompts and file-save messages to use localized web language entries.
- Updated the SquareLine UI project and generated UI sources for the new profile delete control and related UI changes.
- Updated the scale emulator with request/second diagnostics and newer protocol behavior.
- Updated website firmware lookup handling.

### Fixed

- Fixed A&D scale request handling by adding the proper `AD` protocol request bytes.
- Fixed stale calibration profile prompts by tracking prompt start time and fresh scale-read timing.
- Fixed reboot behavior from the web UI.
- Fixed several German/English language gaps and moved duplicated firmware UI language out of system web language files.
- Fixed invalid/corrupt profile diagnostics to report missing files, malformed JSON, incomplete entries, and invalid profile values more clearly.

### Removed

- Removed the experimental `Firmware/RoboTricklerUI-IDF` project files.
- Removed old `DownloadSdFiles` tooling in favor of the SD tar packaging flow.
- Removed obsolete/bundled firmware binary artifacts that were superseded by the current build output.

### Dependencies

- Updated Boards-Manager esp32 - Espressif Systems version `3.3.8` to `3.3.10`.
- Kept the Arduino IDE 1.8.x ESP32 Dev Module board settings mirrored in `.vscode/arduino.json`.
