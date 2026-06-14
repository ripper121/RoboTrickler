# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

RoboTricklerUI is Arduino/ESP32 firmware for a precision powder trickler (used in ammunition reloading). It runs on an ESP32 Dev Module with an LVGL touchscreen UI, RS232 scale communication, stepper motor control via I2S, WiFi, and an SD card web server.

## Build & Upload

**Preferred compile check** (no upload):
```
python tools/compile_upload.py --cli --error --compile-only
```

**Compile and upload via web** (device must be on network as `robo-trickler.local`):
```
python tools/compile_upload.py --cli --error
```

**Compile and flash via serial** (first-time or partition change):
```
python tools/compile_upload.py --cli --error --port COM36 --full
```

The script automatically:
1. Runs `gzip_sd_files.py` (SD-Files → SD-Files-Gz)
2. Runs `create_flash_data.py` (SD-Files-Gz → data/)
3. Runs `create_flash_littlefs_image.py` (data/ → littlefs.bin)
4. Compiles with arduino-cli
5. Uploads firmware and LittleFS over HTTP or serial

**Board settings** (mirrored in `.vscode/arduino.json`):
- Board: ESP32 Dev Module, Espressif ESP32 core `3.3.10`
- Flash: 8MB, DIO mode, 80MHz
- Partition: `default_8MB` (3MB APP / 1.5MB LittleFS)
- CPU: 240MHz, Arduino on Core 1, Events on Core 0

## Testing SD File Changes (Without Reflashing)

Upload a single changed file to the live device via the web editor API:
```powershell
curl.exe -sS -H "Expect:" -F "file=@SD-Files/system/index.html;filename=/system/index.html" "http://robo-trickler.local/system/resources/edit"
```

## Architecture

### Dual-Core Design
- **Core 1 (Arduino loop)**: Scale reads, stepper motor control, WiFi maintenance, trickler state machine
- **Core 0 (LVGL display task)**: All rendering via `disp_task_init()` in `display_task.ino`
- Cores are synchronized via `lvglMutex` (FreeRTOS semaphore). Always acquire this mutex before calling any LVGL function from the main loop.

### State Machine
The main loop dispatches through `TricklerState` (`TRICKLER_IDLE`, `TRICKLER_RUNNING`, `TRICKLER_FINISHED`, `TRICKLER_CALIBRATION_PROMPT`) defined in `RoboTricklerUI.ino`. State transitions happen in `trickler_runtime.ino`.

### Global State
`Config config` (defined in `RoboTricklerUI.ino`) is the single source of truth for all settings and the active trickling profile. It is loaded from `/config.txt` at boot and saved back on changes.

### Filesystem
- **Primary**: SD card (SPI, `SD-Files/` layout)
- **Fallback**: LittleFS (enabled by `ENABLE_LITTLEFS 1` in `compile_options.h`)
- `activeFS` / `ACTIVE_FS` pointer selects the active filesystem at runtime; `activeFSIsSD` tracks which is in use
- **Never edit `data/` or `SD-Files-Gz/` directly** — both are generated from `SD-Files/`

### File Layout

| File | Purpose |
|---|---|
| `RoboTricklerUI.ino` | Globals, `Config` struct, `setup()`, `loop()`, state dispatch |
| `compile_options.h` | Feature switches: `DEBUG`, `ENABLE_LITTLEFS`, `ENABLE_SCREENSHOT` |
| `pindef.h` | All GPIO pin assignments |
| `startup.ino` | `initSetup()` — boot sequence (display → FS → config → profile → scale → WiFi) |
| `trickler_control.ino` | `startTrickler()`, `stopTrickler()`, `startMeasurement()`, `beep()` |
| `trickler_runtime.ino` | Per-state handlers called from the main loop |
| `display_driver.ino` | TFT_eSPI + LVGL init, flush callback |
| `display_task.ino` | FreeRTOS task wrapping LVGL tick/handler on Core 0 |
| `display_helpers.ino` | `updateDisplayLog()`, label helpers, thread-safe UI wrappers |
| `rs232.ino` | Scale serial protocol (GG, AD, KERN, SBI, custom) |
| `sd_config.ino` | `loadConfiguration()`, `saveConfiguration()`, profile I/O |
| `web_server.ino` | HTTP server init, `/update`, `/fwupdate`, OTA handlers |
| `web_api.ino` | REST endpoints: `/getTricklerState`, `/setTarget`, etc. |
| `web_file_editor.ino` | SD file browser/editor API routes |
| `wifi_connection.ino` | Station + AP mode, `maintainWifiConnection()` |
| `stepper.ino` | I2S stepper driver, `stepperBeep()` |
| `profile_actions.ino` | Profile list, load, save, delete |
| `ui.c` / `ui_Screen1.c` | SquareLine Studio-generated LVGL UI objects |
| `ui_events.ino` | LVGL event callbacks wired to hardware actions |
| `ui_dialogs.ino` | `messageBox()` and modal dialog helpers |
| `ui_text.ino` | `langText()` — localization lookup |
| `firmware_check.ino` | OTA version check against remote endpoint |
| `update.ino` | OTA firmware + LittleFS update handlers |

### Localization
All UI strings go through `langText(key)` which reads from `SD-Files/lang/en.json` (or `de.json`). When adding visible text, add the key/value to both language files — never hardcode display strings in firmware.

### SD File Rules
- `fw_update.url` is internal to firmware (`DEFAULT_FW_UPDATE_URL` in `RoboTricklerUI.ino`). Do **not** expose it in SD files, `config.txt`, the config generator UI, or docs. `fw_update.check` may stay user-configurable.
- Files under `SD-Files/system/` are gzip-compressed into `SD-Files-Gz/system/` with `.gz` extension. Files under `SD-Files/profiles/` and `SD-Files/lang/` are copied uncompressed (parsed directly by firmware).
- The web server transparently serves `.gz` variants when they exist.

### Library Dependencies (check before writing new code)
- `C:\Users\ripper121\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.10\libraries`
- `C:\Users\ripper121\Documents\Arduino\libraries`

Reuse existing ESP-IDF/Arduino library solutions before implementing new code.
Try always to save as much Heap as possible, because the Webserver, Wifi and LVGL are heap hungry.
Try to reuse UI Code to save Heap.