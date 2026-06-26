# Changelog

## 2.14

### Trickling model: weight-per-revolution
- The per-stepper metric changed from `unitsPerThrow` to **weight per revolution** (`weightPerRev`). The trickle math now computes `steps = remainingWeight * motorStepsPerRev / weightPerRev` (`trickler_runtime.ino`).
- Steps-per-revolution is now **configurable** via `stepper.stepsPerRev` in `config.txt` (`config.motorStepsPerRev`, default 200) instead of the hard-coded `MOTOR_REV_STEPS`.
- The stepper `speed` field was renamed to `rpm` everywhere (config, profiles, runtime, web editor).

### Config & profile file format (breaking)
- `config.txt` restructured into nested objects: `wifi.*` (incl. new `wifi.enabled`), `scale.*`, `stepper.stepsPerRev`, `totalCounter.{enable,count}`, `firmwareUpdate.check`, `activeProfile`.
- Profiles restructured: a `general` block, a `stepper` map keyed `"1"`/`"2"` (`enabled`/`weightPerRev`/`rpm`), and a `trickleMap[]` of `{diffWeight, measurements, stepper:{id,steps,rpm}}`.
- Profile-only fields are now reset before a profile is applied, so values can't leak between profiles (`loadProfile`).
- Config/profile loading reworked for lower heap use and faster parsing.

### Calibration profile robustness
- The calibration profile is now hardware-independent: it stores its throw as motor **revolutions** and converts to steps via `motorStepsPerRev` at load time.
- If `calibrate.txt` is missing or corrupt it is **auto-regenerated** from a built-in template instead of failing the boot (`writeDefaultCalibrateProfile`/`ensureCalibrateProfile`).
- Profile writes (target-weight save, profile creation) are now **atomic** (write to `.tmp`, then rename).
- "Create profile from calibration" derives `weightPerRev` from the measured weight and builds the `trickleMap` with an RS232 limit factor.

### Runtime / display
- Start now always **reloads the selected profile from disk** so a run uses current on-disk values.
- Counters split into a persisted **total counter** (`totalCounter`) and a per-run **session counter** (`sessionCount`).
- Negative weight values now display correctly; weight uses a `NaN` sentinel instead of `-1`, and the weight label color resets on stop.

### Storage & stability
- SD-card speed and timing fixes; LittleFS space savings.
- Runtime watchdogs disabled where long operations could trip them.

### Performance / heap
- Profile list moved from `String[32]` to a fixed `char[32][32]` buffer, avoiding up to 32 heap allocations per rebuild.
- LVGL draw buffer increased from 8 to 16 rows.

### Naming & tooling
- Source files renamed: `pindef.h`→`hardware_pins.h`, `rs232.ino`→`scale_rs232.ino`, `sd_config.ino`→`sd_storage.ino`, `update.ino`→`ota_update.ino`; `flash_filesystem.ino` reorganized into `filesystem_mount.ino`.
- Build tools renamed/reorganized: `compile_upload.py`→`firmware_build_upload.py`, `gzip_sd_files.py`→`filesystem_generate_sd_trees.py`, `create_flash_*`→`filesystem_stage_littlefs_data.py`/`filesystem_build_littlefs_image.py`, `update_usb_flash.py`→`release_update_usb_flash.py`.
- Broad identifier cleanup (e.g. `steppers`→`stepper`, UPPER_CASE globals → camelCase).

### WiFi
- Added a **Wi-Fi enable/disable toggle** on the display (`config.wifiEnabled`, `ui_ButtonWifi`), with `stopWifiServices()`/`applyWifiEnabled()` to bring the web server, DNS and mDNS up or down at runtime.
- Added **Wi-Fi QR-code** generation/display for the setup AP (`wifi_qr.ino`, `qrcode` library); AP password now lives in a fixed buffer.

### Filesystem
- **Sync files between Flash (LittleFS) and SD** directly from the display, via two on-screen buttons, copying `config.txt` and all profiles atomically (`filesystem_sync.ino`).
- SD and LittleFS can be **mounted simultaneously**, tracked independently (`sdMounted`/`littleFsMounted`); `activeFs` selects which serves runtime reads/writes.

### UI
- **Cycle the scale protocol on the device** (`nextScaleProtocol`, empty protocol shown as `STREAM`, `ui_ButtonScaleProtocol`).
- Multiple dialog bug fixes and button repositioning; `cancelInteractiveDialogs()` replaces the single delete-confirm cancel.
- Removed the BMP boot **splash screen** (`splash.ino`).

### Other
- Removed the in-browser profile generator page.
