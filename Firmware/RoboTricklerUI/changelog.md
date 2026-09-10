# Changelog

## 2.14

### Highlights
- Reworked the trickling model from `unitsPerThrow` to configurable **weight per revolution** (`weightPerRev`).
- Added device-side Wi-Fi enable/disable, scale-protocol cycling, setup-AP QR code display, and Flash/SD synchronization controls.
- Reorganized SD/LittleFS packaging and renamed the SD-hosted profile editor to `profile_editor.html`.
- Centralized weight formatting, entry, and comparison around the `0.000` to `500.000` domain.
- Reduced heap churn in profile lists, UI text updates, web responses, dialogs, and scale parsing.
- Moved the stepper shift-register drive to the ESP32 I2S peripheral with DMA.

### Breaking config/profile changes
- `config.txt` now uses nested objects for `wifi`, `scale`, `stepper`, `totalCounter`, and `firmwareUpdate`.
- `wifi.enabled` controls Wi-Fi services at runtime.
- `stepper.stepsPerRev` replaces the hard-coded motor step count.
- `activeProfile` replaces the old flat profile field.
- `firmwareUpdate.check` remains user-configurable; the update URL stays internal to firmware.
- Profiles now use `general`, a `stepper` map keyed by `"1"`/`"2"`, and `trickleMap[]` entries with `diffWeight`, `measurements`, and nested stepper settings.
- Per-stepper `speed` fields were renamed to `rpm`.
- Per-stepper calibration changed from `unitsPerThrow` to `weightPerRev`.
- The config reader requires its exact current schema. Normal profiles accept the documented compact schema and fill omitted optional fields with defaults while still rejecting unknown fields.
- `/getProfileList` now returns a JSON array instead of the legacy numeric-key object.

### Added
- Added compact normal-profile support with defaults for omitted optional general, stepper, and trickle-map fields.
- Added per-profile `general.trickleMapLimitFactor` tuning with automatic Stepper 1 trickle-map recalculation.
- Added on-device tuning of each `trickleMap[].stepper.steps` value using the same per-entry flow as measurement tuning.
- Added required per-entry `stepper.reverse` direction control to `trickleMap` profiles and the web profile editor.
- Added `compile_options.h` for compile-time feature switches.
- Added simultaneous SD and LittleFS mounting with `activeFs` runtime selection.
- Added Flash-to-SD and SD-to-Flash sync for `config.txt` and profiles, using atomic temporary-file replacement.
- Added LittleFS/SD tree generation tooling and release USB-flash tooling.
- Added automatic regeneration of the calibration profile when `calibrate.txt` is missing or invalid.
- Added profile creation/tuning flow based on measured calibration weight and generated trickle-map entries.
- Added on-device scale-protocol cycling with `STREAM` display for empty protocol mode.
- Added setup-AP Wi-Fi QR-code generation and display using a fixed module buffer.
- Added asynchronous Wi-Fi scanning for the setup page so scans do not stall the display task.
- Added localized firmware web status pages backed by SD-hosted `web.firmware.*` language keys.

### Changed
- Updated the build/upload tooling for Arduino-ESP32 3.3.11 and its bundled esptool 5.3.1.
- Save now commits all profile tuning modes together; explicit step edits take precedence over weight/rev recalculation, with existing map entries preserved.
- Unified profile tuning into one shared dialog with left/right arrows to cycle weight/rev, measurements, and steps.
- Renamed source files to the current naming convention, including `pindef.h` to `hardware_pins.h`, `rs232.ino` to `scale_rs232.ino`, `sd_config.ino` to `sd_storage.ino`, and `update.ino` to `ota_update.ino`.
- Split filesystem mounting and sync logic into `filesystem_mount.ino` and `filesystem_sync.ino`.
- Reorganized SD assets into `SD-Files`, `SD-Files-Gz`, `SD-Files-LittleFS`, and `SD-Files-Legacy`.
- Replaced the old in-browser profile generator/profile editor pages with the updated profile editor and settings pages.
- Updated bundled SD web pages, shared CSS, language files, profile defaults, and manual content.
- Changed startup/runtime storage so profile-only values are reset before loading a new profile.
- Changed Start to reload the selected profile from disk before running.
- Changed target-weight and profile web edits to be rejected while the trickler is running.
- Changed captive-portal handling so DNS service continues from the display task.
- Changed profile list storage from `String[32]` to a fixed `char[32][32]` buffer.
- Changed weight display/entry precision to three decimals.
- Changed invalid/unknown weight state from `-1` to `NaN`.
- Changed the LVGL draw buffer from 8 rows to 16 rows.
- Changed dialog creation to shared factory helpers and dynamic dialogs.
- Changed UI font and dialog button sizing for the current display layout.

### Fixed
- Fixed on-screen target-weight edits being ignored when Start was pressed.
- Fixed profile-step RPM carryover during bulk moves.
- Fixed `targetWeight` leaking from a previous profile when loading a profile without a target weight.
- Fixed mid-run web target/profile edits racing the active trickle run.
- Fixed several trickler edge cases by routing profile-step selection through the shared weight-comparison helper.
- Fixed negative scale values and weight-label reset behavior.
- Fixed scale parsing to reduce heap use and avoid stale unit text.
- Fixed total-counter writes so config is not rewritten when no completed throw changed the count.
- Fixed profile save and target-weight save paths to use atomic writes.
- Fixed Wi-Fi setup and AP web behavior after Wi-Fi service state changes.
- Fixed profile tuning measurement handling.
- Fixed dialog cancellation and confirmation state issues.
- Fixed Stop so an active I2S/DMA stepper move is cancelled promptly and cannot resume after a quick restart.
- Fixed repeated Start presses from rewriting an unchanged profile target weight by tracking unsaved target edits, and kept calibration profiles free of unsupported target-weight fields.
- Fixed SD file and manual content to match the current firmware/web UI.

### Removed
- Removed the BMP boot splash screen and related `splash.ino`.
- Removed obsolete `data/` LittleFS staging output from the firmware tree.
- Removed old profile generator assets.
- Removed obsolete generated binary artifacts from `SD-Files-Gz` when they are regenerated by tooling.
- Removed dead helper code and deduplicated profile, language, and dialog helpers.
- Removed old build script names in favor of the `tools/filesystem_*`, `tools/firmware_build_upload.py`, and `tools/release_update_usb_flash.py` naming.
