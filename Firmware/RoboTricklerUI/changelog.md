# Changelog Firmware 2.12

Compared to firmware `2.11` (`7a8023b764e71e313d1028ca00592c1b6466e495`).

### Overview
- Updated firmware version from `2.11` to `2.12`.
- Reworked the firmware into smaller runtime, display, startup, profile, web, Wi-Fi, and update modules.
- Improved RS232 scale handling, especially request generation, response parsing, and A&D scale support.
- Added display polish and maintenance features, including the SD logo splash, optional screenshots, profile deletion, and profile tuning.
- Updated the SD-hosted web tools, SD packaging flow, and Arduino ESP32 core setup for `3.3.10`.

### Added

- Added built-in RS232 request commands for `GG`, `AD`, `KERN`, `KERN-ABT`, `KERN-ABS`, and `SBI` scale protocols.
- Added stricter RS232 response parsing that reads complete numeric tokens and rejects ambiguous scale lines instead of merging unrelated digits.
- Added optional debug logging for custom RS232 transmit data in both display-safe text and hex forms.
- Added an explicit trickler state machine for idle, running, finished, and calibration-prompt states.
- Added weight comparison scaling (`WEIGHT_SCALE_FACTOR`) to make trickling/tolerance decisions less sensitive to float rounding.
- Added profile tuning support for `unitsPerThrow`, including localized display text and stop-while-running protection.
- Added profile delete support with confirmation, calibrate-profile protection, and automatic profile list refresh.
- Added `/system/logo.bmp` splash logo support during startup.
- Added optional BMP screenshot capture support for the LVGL display and `/screenshot` route when enabled.
- Added `/getTricklerState` so the SD-hosted trickler page can sync weight and running state in one request.
- Added localized fallback/error strings for corrupt config/profile handling and profile validation errors.
- Added localized web editor/file editor strings for reboot, upload, save, delete, preview, download, refresh, and expand/collapse actions.
- Added `tools/compile_upload.py` as the single build/upload entry point for regenerating gzip files, building firmware and LittleFS with Arduino CLI at Core Debug Level Error, and uploading through the web server or a COM port.
- Added `tools/create_sd_tar.py`, `tools/gzip_sd_files.py`, `SD-Files-Gz`, and `SD-Files-Gz.tar` support for compressed SD-file packaging.
- Added updated `Hardware/CAD` model exports and moved older CAD/3D assets under `Hardware/CAD/OldVersion`.

### Changed

- Updated the firmware update URL default in firmware code and kept firmware URL handling internal to firmware.
- Updated ESP32 Arduino core references and VS Code include paths from `3.3.8` to `3.3.10`.
- Changed firmware `DEBUG` default to `0`.
- Changed LVGL display buffering and reduced draw rows to lower memory pressure.
- Changed startup order so SD-card errors can still initialize the UI before showing blocking messages.
- Changed runtime handling so periodic Wi-Fi maintenance and display/runtime stats are centralized in the main loop.
- Changed display logging so `noLog` status messages update the info label without appending to the scrollback log.
- Changed done-with-count display text to use localized `status_count`.
- Changed the web server to serve `.gz` SD files transparently when compressed copies exist.
- Changed the SD-hosted trickler page to prevent profile changes while running and to recover from slow/failed state requests.
- Changed the SD web editor to load language data before building controls and to hide `/lang` from the root file tree.
- Simplified the Ace editor bundle by keeping only the modes/workers used by the SD editor.
- Improved corrupt config/profile messages by routing them through language strings.
- Simplified language loading so firmware falls back through `/lang/<language>.json` and `/lang/en.json`.
- Updated default SD config values to blank Wi-Fi credentials, `GG` scale protocol, and done-only beeper behavior.
- Updated profile generator/editor pages to localize units, section names, actuator labels, profile-step labels, and filename hints.
- Updated web reboot prompts and file-save messages to use localized web language entries.
- Updated the SquareLine UI project and generated UI sources for profile delete/tune controls, tab indicator polish, and related UI changes.
- Updated the scale emulator with request/second diagnostics and newer protocol behavior.
- Updated website firmware lookup handling.
- Updated the manual for firmware `2.12`, SD structure, compressed SD files, web editor upload flow, and supported scale protocol notes.

### Fixed

- Fixed A&D scale request handling by adding the proper `AD` protocol request bytes.
- Fixed stale calibration profile prompts by tracking prompt start time and fresh scale-read timing.
- Fixed reboot behavior from the web UI so the browser no longer depends on a completed navigation response.
- Fixed web firmware upload completion/error handling by tracking update start/success state explicitly.
- Fixed JSON/profile list output escaping for web API responses.
- Fixed several German/English language gaps and moved duplicated firmware UI language out of system web language files.
- Fixed invalid/corrupt profile diagnostics to report missing files, malformed JSON, incomplete entries, and invalid profile values more clearly.
- Fixed profile save/delete/tune edge cases around missing calibrate profile data and protected profile files.
- Fixed Wi-Fi status display and reconnect behavior by centralizing Wi-Fi service startup and DNS/mDNS handling.
- Fixed SD editor multipart upload reliability documentation by requiring `Expect:` suppression for test uploads.

### Removed

- Removed the experimental `Firmware/RoboTricklerUI-IDF` project files.
- Removed old `DownloadSdFiles` tooling in favor of the SD tar/gzip packaging flow.
- Removed obsolete root-level `compile_upload.py` and `create_sd_tar.py` copies after moving them under `tools/`.
- Removed unused Ace editor extensions, snippets, image resources, and modes from the SD-hosted editor bundle.
- Removed obsolete/bundled firmware binary artifacts that were superseded by the current build output.
- Removed the old monolithic `helper.ino`, `ui.ino`, and `webserver.ino` files after splitting their code into focused modules.

### Dependencies

- Updated Boards-Manager esp32 - Espressif Systems version `3.3.8` to `3.3.10`.
- Kept the Arduino IDE 1.8.x ESP32 Dev Module board settings mirrored in `.vscode/arduino.json`.
- Updated the bundled LVGL config for the current Arduino ESP32 core/library setup.
