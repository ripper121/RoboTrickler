# Firmware-Update-Info

**Um die neueste Firmware mit allen Funktionen nutzen zu können, muss ein USB-Update durchgeführt werden.**

1. Robo-Trickler per USB anschließen.
2. `flash.bat` aus der Datei `USB-Flash.zip` starten.
3. SD-Karte mit FAT32 formatieren.
4. Dateien aus `SD-Files.zip` auf die SD-Karte kopieren.

Wer kein USB-Flash durchführen möchte, kann die Datei `SD-File-Legacy.zip` nutzen. Diese funktioniert auch mit Firmwares vor Version 2.13.

Hinweis: In diesem Fall ist die Nutzung ohne SD-Karte nicht möglich.


# Changelog Firmware 2.13


### Overview

- Added Wi-Fi enable/disable controls, connection QR codes, and improved network recovery.
- Added scale protocol selection from the touchscreen.
- Reduced heap use and improved UI/dialog reliability.
Only with USB-Update:
- Added LittleFS support so the firmware can run from internal flash without an SD card.
- Added on-device synchronization of configuration and profiles between LittleFS and SD.


### Added

- Added a persistent `wifi.enabled` setting to disable Wi-Fi services when they are not required.
- Added touchscreen Wi-Fi controls and QR codes for the configured network, access point, and web interface.
- Added an access-point setup page for devices without working station credentials.
- Added touchscreen scale protocol cycling for `GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM`, and streaming scales.
- Added semantic error, warning, and success dialogs with shared confirmation handling.
- Added compile-time feature switches in `compile_options.h`.
- Added tools to build LittleFS images, gzip SD assets, create 8 MB merged firmware images, and update USB-flash release files.
Only with USB-Update:
- Added an internal LittleFS image containing the default configuration, profiles, languages, and web interface.
- Added automatic filesystem selection at startup, preferring SD when available and falling back to LittleFS.
- Added touchscreen actions to copy `config.txt` and profile files from flash to SD or from SD to flash.
- Added confirmation, progress, completion, and error messages for filesystem synchronization.

### Changed

- Changed the flash layout and release image to support the 8 MB board configuration and a dedicated LittleFS partition.
- Changed config, profile, language, web, and update file access to use the active filesystem instead of assuming an SD card.
- Changed firmware updates to preserve or restore the internal filesystem where required.
- Changed profile storage and several UI text paths from dynamic `String` allocations to fixed buffers or static text to reduce heap fragmentation.
- Reworked dialogs so message, confirmation, profile tuning, and filesystem sync actions share reusable UI code.
- Reworked generated UI elements and button placement for filesystem sync, Wi-Fi, scale protocol, profile actions, target weight, and trickler controls.
- Improved target-weight formatting and active trickling feedback.
- Improved web server file handling for LittleFS, compressed assets, uploads, downloads, and filesystem reporting.
- Improved Wi-Fi startup, reconnect, mDNS, DNS, and service-state handling.
- Updated English and German firmware/web translations for the new controls and status messages.
- Updated default profiles and the profile generator for the current calibration and trickling behavior.
- Updated build documentation and board settings for Espressif ESP32 core `3.3.10`.

### Fixed

- Fixed UI dialog crashes, stale modal state, unresponsive blocking dialogs, and confirmation-button cleanup.
- Fixed profile tuning and deletion interactions when another dialog is open or trickling starts.
- Fixed filesystem sync controls being shown when SD or LittleFS is unavailable.
- Fixed custom and streaming scale protocols so they do not send an unintended built-in request.
- Fixed Wi-Fi QR positioning, sizing, localization, and refresh behavior.
- Fixed Wi-Fi toggling and reconnect behavior after configuration changes.
- Fixed target and live-weight labels retaining stale text or colors after stopping.
- Fixed profile list handling and several temporary UI allocations that could exhaust or fragment heap.
- Fixed web API and UI edge cases discovered during the LittleFS and touchscreen-control changes.

### Removed

- Removed duplicate and obsolete generated UI helper code.
- Removed obsolete SD packaging and build artifacts superseded by the LittleFS/8 MB release workflow.

### Dependencies

- Uses Boards-Manager esp32 - Espressif Systems version `3.3.10`.
- Uses the ESP32 `LittleFS` implementation included with the selected core.
- Keeps LVGL widgets disabled and retains the existing LVGL memory allocation.
