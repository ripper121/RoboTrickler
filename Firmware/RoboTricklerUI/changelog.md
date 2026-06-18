# Firmware-Update-Info

**Um die neueste Firmware mit allen Funktionen nutzen zu können, muss ein USB-Update durchgeführt werden.**

1. Robo-Trickler per USB anschließen.
2. `flash.bat` aus der Datei `USB-Flash.zip` starten.
3. SD-Karte mit FAT32 formatieren.
4. Dateien aus `SD-Files.zip` auf die SD-Karte kopieren.

Wer kein USB-Flash durchführen möchte, kann die Datei `SD-File-Legacy.zip` nutzen. Diese funktioniert auch mit Firmwares vor Version 2.13.

Hinweis: In diesem Fall ist die Nutzung ohne SD-Karte nicht möglich.

Bei Fehlern oder Fragen, einfach melden: [Kontakt](https://shop.strenuous.dev/contact)


# Changelog Firmware 2.14

### Overview
- Added on-device control over Wi-Fi (enable/disable), scale protocol cycling, the Wi-Fi QR code, and SD ↔ LittleFS file synchronization.
- Reworked the dual-filesystem handling so SD and LittleFS can be mounted at the same time, with the SD firmware/LittleFS update now running early enough to flash from a bare SD card.
- Improved speed and memory usage: faster SD SPI clock, streamed web responses, stack-buffer profile/state handling, and a minimal LittleFS web bundle.
- Replaced the browser profile/config generators with a `create_profiles.py` tool and reworked the message/confirmation dialog system.
- Updated the manual, USB-flash tooling, and SD packaging (legacy 4 MB layout support).

### Added

- Added a Wi-Fi enable/disable toggle on the display, backed by a new `wifi.enabled` config field that defaults to on.
- Added on-device scale-protocol cycling via a button that steps through `STREAM`, `GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, and `CUSTOM`, with a label showing the current protocol.
- Added a Wi-Fi QR code on the display with a "Click to Hide" action.
- Added SD ↔ LittleFS file synchronization ("Upload Flash > SD" / "Download SD > Flash") with confirmation dialogs, a copied-file count, and an automatic restart to load the synced SD files.
- Added a new `settings.html` web page.
- Added early-boot SD firmware/LittleFS updating so a card containing only `firmware.bin` / `littlefs.bin` can flash the device, with progress shown on the display.
- Added typed dialog helpers (`errorBox`, `successBox`, `warnBox`, `confirmBox`, `showConfirmBox`) and `cancelInteractiveDialogs()` to centralize and standardize message boxes.
- Added negative/`NaN` weight handling so unstable or unreachable-scale readings show as `NaN...` instead of a fake `-1.0`.
- Added finer calibration trickle maps (8 difference-weight steps instead of 5) for smoother approach to target.
- Added `tools/create_profiles.py` to generate and upload trickler profiles through the web file-editor API (single, bulk, or local dry-run).
- Added a minimal `SD-Files-LittleFS` web bundle (slimmed editor, `.gz`-only assets) for the LittleFS fallback filesystem.
- Added legacy 4 MB / `min_spiffs` packaging support (`--legacy`) and a `--prod` mode to the USB-flash tooling.
- Added `userTracker.php` and a `CLAUDE.md` project guide.

### Changed

- Increased the SD SPI clock from 4 MHz to 20 MHz for faster card access.
- Reworked filesystem mounting so SD and LittleFS are tracked independently (`sdMounted` / `littleFSMounted`) and both can be mounted simultaneously.
- Streamed the `/fwupdate`, `/getProfileList` web responses and built `/getTricklerState` in a stack buffer to reduce heap pressure.
- Stored the profile list in fixed `char` buffers (`PROFILE_LIST_MAX`, `strcmp`/`strlcpy`) instead of `String` objects, fixing several comparison edge cases and saving heap.
- Rejected web `/setProfile` and `/start` while the trickler is running or the profile is invalid (HTTP 409).
- Throttled scale-timeout logging so it updates the status line every cycle but only adds one scrollback entry per outage.
- Reset the weight label color to white on stop and routed target-weight label updates through a single `updateTargetWeightLabel()` helper.
- Enabled the runtime watchdog disable during setup.
- Added a `motorRevSteps` config field (default `200`) for the stepper's steps per revolution, used by the bulk move and calibration math.
- Renamed the profile actuator fields `unitsPerThrow`/`unitsPerThrowSpeed` to `unitsPerRev`/`unitsPerRevSpeed` (and the on-device tuning label to `units/rev`). Existing profiles must be re-saved with the new names.
- Updated the manual for firmware `2.14`.

### Fixed

- Fixed stale dialogs lingering during web-triggered actions.
- Fixed a false running-state toggle button after a failed startup.
- Fixed repeated web starts and profile changes while the trickler was running.
- Fixed dialog and long-label overflow, including invalid-profile message clipping.
- Fixed cross-core log-buffer and debug LVGL access races.
- Fixed the weight color not resetting after a stop.
- Fixed the offline SD `start.html` page.
- Removed leftover pre-2.13 root profile files (`/avg.txt`, `/calibrate.txt`) on boot.

### Removed

- Removed the browser profile generator and config generator pages (superseded by the profile editor and `create_profiles.py`).
- Removed the SD splash logo (`splash.ino`, `showSplashLogo`, `logo.bmp`).
- Removed the dead generated UI sources `ui_helpers.c`, `ui_helpers.h`, and `ui_comp_hook.c`.
- Removed the bundled `profiles/full.txt` sample profile.

### Dependencies

- Updated the bundled LVGL configuration (`lv_conf.h`).
- Kept the ESP32 Dev Module / Espressif `3.3.10` board settings mirrored in `.vscode/arduino.json`.
