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
- SD and LittleFS can be mounted **simultaneously**: `sdMounted` / `littleFSMounted` track each independently. The `activeFS` / `ACTIVE_FS` pointer selects which one serves runtime reads/writes, and `activeFSIsSD` says which that is (SD preferred when present).
- `filesystem_sync.ino` copies files both ways ("Upload Flash > SD" / "Download SD > Flash") from the display, then restarts.
- **Never edit `data/`, `SD-Files-Gz/`, or `SD-Files-LittleFS/` directly** — all are generated from `SD-Files/`

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
| `wifi_qr.ino` | Wi-Fi QR-code generation/display |
| `stepper.ino` | I2S stepper driver, `stepperBeep()` |
| `profile_actions.ino` | Profile list, load, save, delete |
| `flash_filesystem.ino` | SD/LittleFS mounting, legacy-file cleanup |
| `filesystem_sync.ino` | SD ↔ LittleFS file synchronization (display action) |
| `ui.c` / `ui_Screen1.c` | SquareLine Studio-generated LVGL UI objects |
| `ui_events.ino` | LVGL event callbacks wired to hardware actions |
| `ui_dialogs.ino` | `messageBox()` + typed dialog helpers (`errorBox`, `successBox`, `warnBox`, `confirmBox`, `cancelInteractiveDialogs()`) |
| `ui_text.ino` | `langText()` — localization lookup |
| `firmware_check.ino` | OTA version check against remote endpoint |
| `update.ino` | OTA firmware + LittleFS update handlers |
| `screenshot.ino` | Display screenshot endpoint (gated by `ENABLE_SCREENSHOT`) |

### Localization
All UI strings go through `langText(key)` which reads from `SD-Files/lang/en.json` (or `de.json`). When adding visible text, add the key/value to both language files — never hardcode display strings in firmware.

### SD File Rules
- `fw_update.url` is internal to firmware (`DEFAULT_FW_UPDATE_URL` in `RoboTricklerUI.ino`). Do **not** expose it in SD files, `config.txt`, or docs. `fw_update.check` may stay user-configurable.
- Files under `SD-Files/system/` are gzip-compressed into `SD-Files-Gz/system/` with `.gz` extension. Files under `SD-Files/profiles/` and `SD-Files/lang/` are copied uncompressed (parsed directly by firmware).
- The web server transparently serves `.gz` variants when they exist.

### Library Dependencies (check before writing new code)
- `C:\Users\ripper121\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.10\libraries`
- `C:\Users\ripper121\Documents\Arduino\libraries`

Reuse existing ESP-IDF/Arduino library solutions before implementing new code.
Try always to save as much Heap as possible, because the Webserver, Wifi and LVGL are heap hungry.
Try to reuse UI Code to save Heap.

## Maintaining the User Manual (`manual.md`)

`manual.md` is the German end-user manual, mirrored on the GitHub wiki (e.g. page `Anleitung-Firmware-2.14`). When firmware behavior changes, update the manual using this process — **verify every statement against the code, never from memory.**

### Update checklist
1. **Version line**: keep `Stand: Firmware X.XX` in sync with `FW_VERSION` in `RoboTricklerUI.ino`. Check `changelog.md` for what changed in the release.
2. **Config fields**: cross-check every `config.txt` field documented in the *Konfiguration* section against `loadConfiguration()` / `saveConfiguration()` in `sd_config.ino`. Every field the firmware reads/writes must be documented with its default values; nothing extra.
3. **Profile fields**: cross-check the *Aufbau eines Profils* section against `readProfile()` / `loadProfileEntries()` / `loadProfileEntry()` in `sd_config.ino` (`general`, `actuator.stepperN`, `rs232TrickleMap[]`, plus the `calibrate` shape).
4. **Display UI**: derive what's possible on the touchscreen from the event callbacks in `ui_events.ino`, `filesystem_sync.ino`, `ui_dialogs.ino` and their button wiring in `ui_Screen1.c` (`lv_obj_add_event_cb`). Confirm which tab a control lives on via its parent panel (`ui_PanelPageInfo` = Info tab, etc.). Runtime behavior (auto-refill, weight colors, over-trickle alarm, counters, diagnostics line) comes from `trickler_runtime.ino` and `trickler_control.ino`.
5. **Web features**: list HTTP endpoints from the `server.on(...)` registrations in `web_server.ino`; the browser remote-control, `settings.html`, `profileEditor.html`, and file editor are the SD pages under `SD-Files/system/`. Bulk profile generation now lives in `tools/create_profiles.py` (uploads via the web file-editor API), not a browser generator page.
6. **Scales**: the protocol list and request commands come from `SCALE_REQUEST_COMMANDS` in `rs232.ino` (empty protocol is shown as `STREAM`).

### Conventions to keep consistent
- **Table of contents**: the *Inhaltsverzeichnis* lists all `#` and `##` headings. GitHub anchors = lowercase, spaces→`-`, punctuation dropped (`G&G`→`#gg`, `(LittleFS)`→`…littlefs`), umlauts kept, consecutive dropped chars leave double hyphens (`Gramm / Grain`→`#gramm--grain`). Update the TOC whenever headings change.
- **Heading hierarchy**: top-level chapters are `#`, subsections `##`. All scale brands (G&G, Sartorius, Kern, A&D, Steinberg) are `##` under `# Waagen` — keep them at the same level.
- **Screenshot placeholders**: use the blockquote form `> 📸 **Screenshot – Display:** …` or `> 📸 **Screenshot – Webserver:** …`, naming the source and what the shot must contain. Search the file for `📸` to find open placeholders.
- Do **not** expose `fw_update.url` (see SD File Rules).

### How we went through it (June 2026, 2.13)
Reference for how the current manual was brought up to date:
1. Bumped `2.12`→`2.13` and reconciled every section with the code (config/profile fields, LittleFS, WiFi toggle + AP setup, on-device scale-protocol cycling, filesystem sync, 8 MB USB-flash command).
2. Audited the firmware + SD-files for user-facing features missing from the manual, then added the high-value ones (auto-refill dauerbetrieb, weight colors, profile-tab lock, web remote control, over-trickle alarm, trickle counters, captive portal, FW-update popup, storage indicator, tuning diagnostics line, web localization). Deliberately left out internal-only items (screenshot endpoint, `compile_options.h` switches).
3. Added the *Inhaltsverzeichnis* with anchor links and a new *Bedienung am Display* chapter.
4. Added `📸` screenshot placeholders for the new 2.13 features and the web pages lacking images.
5. Fixed heading hierarchy so all scale brands sit under `# Waagen`.