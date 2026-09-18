# ESP-IDF 5.5.5 code review

Reviewed: 2026-09-18. Target: ESP32 Dev Module, Arduino ESP32 core 3.3.11. The installed core's `esp_idf_version.h` reports ESP-IDF 5.5.5, so the local documentation matches the IDF libraries used by this build. This is a source review, not a claim that every Arduino wrapper implements the IDF examples verbatim. In particular, ESP-IDF does not require replacing Arduino `WiFi`, `SD`, `WebServer`, `Update`, or LVGL with native IDF components solely for conformance.

Documentation root: `C:\Users\ripper121\Documents\GitHub\RoboTrickler\Doc\esp-idf-en-v5.5.5`. Links below point to its rendered HTML pages. Source lines are relative to this folder.

Priorities below identify changes recommended for reliable operation with IDF 5.5.5.

**Project decision:** No HTTP authentication, restrictions on file downloads, or encryption are needed. Secure Boot, flash encryption, HTTPS/TLS, and signed images are outside this review's recommended work. The current HTTP file access and update behavior is intentional; the report does not recommend changing it for access control. The setup AP password derived from the factory MAC address is accepted and needs no change. Disabling the watchdog is intentional; this review does not recommend enabling it. Early SD OTA is the intended last recovery path, so an application rollback self-test is not requested.

Review coverage: all 33 sketch/C/header source files in this folder, plus build/release tools and SD-hosted web assets where they touch the firmware's IDF-facing behavior. Focus areas were direct IDF calls, FreeRTOS synchronization, I2S, OTA/partitions, filesystems, Wi-Fi, HTTP endpoints, startup, hardware pins, and heap use. Existing solutions were checked in the installed ESP32 core examples, the local Arduino libraries, and the supplied IDF documentation. UI layout, translation wording, and calibration accuracy require separate product or hardware validation; they are not specified by ESP-IDF.

## Remaining findings and recommended changes

| Priority | Location | Finding and recommended change | Documentation |
| --- | --- | --- | --- |
| Critical | `stepper.ino`, `hardware_pins.h` | `auto_clear = true` makes the I2S driver transmit **zero** on a TX underrun. Bit 0 is the active-high motor-disable output; zero enables the motors. An underrun can also change direction and truncate a step pulse. Make the *electrical* idle/underrun state motor-disabled through the actual board wiring or a separate output gate, then test starvation and reboot on the board. The new write-fault latch stops later moves, but software refilling cannot guarantee a safe state if the feeder stops. The current core enables `CONFIG_I2S_ISR_IRAM_SAFE`, which mitigates the guide's specific flash-cache interrupt delay; retest if the build configuration changes. | [I2S DMA and IRAM safety](../../Doc/esp-idf-en-v5.5.5/api-reference/peripherals/i2s.html) |

## Conditional changes and checks

1. **Core and build settings:** Verify the compiled partition CSV and I2S Kconfig values for each release. The core's `default_8MB.csv` has two `0x330000` OTA app slots and one `0x180000` SPIFFS-subtype data partition, which is consistent with the runtime partition lookup. The code's firmware limit resolves to the smaller actual partition size. The installed core enables rollback and the I2S IRAM-safe ISR. Arduino's default `verifyOta()` immediately confirms the image. The project relies on its early SD OTA path rather than an application rollback self-test. In `startup.ino`, SD OTA runs before stepper and configuration initialization, but after display and filesystem initialization; those stages must succeed for this recovery path to run. See [partition tables](../../Doc/esp-idf-en-v5.5.5/api-guides/partition-tables.html).
2. **Shared configuration:** The Wi-Fi and stepper status flags now use atomic booleans. `Config` is still accessed from both the LVGL task and Arduino loop. The UI blocks target changes while running, but a full cross-task audit of each editable field and its writer/reader timing is still needed before claiming this structure is synchronized. See [IDF FreeRTOS critical sections](../../Doc/esp-idf-en-v5.5.5/api-reference/system/freertos_idf.html#critical-sections).
3. **Hardware limits:** Check `hardware_pins.h` against the actual board schematic and the ESP32 strapping/input-only GPIO rules before changing the map. In particular GPIO0, GPIO2, GPIO12, and GPIO15 affect boot strapping; GPIO34-39 are input-only. A software review cannot establish the board's pull resistors or safe motor power-up state. See [ESP32 GPIO and strapping pins](../../Doc/esp-idf-en-v5.5.5/api-reference/peripherals/gpio.html) and the ESP32 hardware reference.

## Practices already aligned with the docs

- I2S uses the current channel/standard-mode driver (`i2s_new_channel`, `i2s_channel_init_std_mode`, `i2s_channel_enable`, `i2s_channel_write`) and checks most initialization returns. Its 256-frame stereo 32-bit DMA buffer is 2048 bytes, below the documented 4092-byte DMA limit. The mutex also protects the application's shared batch buffer.
- The ESP32 task stacks passed to `xTaskCreatePinnedToCore()` are byte counts, consistent with IDF FreeRTOS. The LVGL display task uses a recursive mutex around `lv_timer_handler()`.
- Firmware OTA targets `esp_ota_get_next_update_partition()` and checks the selected partition size. The configured 8 MB partition table includes `otadata`, `ota_0`, and `ota_1`.
- Filesystem operations generally use `FilesystemLockGuard`. Both web and SD LittleFS updates now unmount the partition before writing; the SD path remounts it after a failed update.

## File-by-file review map

The map distinguishes IDF-facing behavior from application or LVGL behavior. A file listed without a finding does not need an IDF-specific change found in this review; it is not a guarantee of complete functional correctness.

| Files reviewed | Documentation or contract checked | Result |
| --- | --- | --- |
| `RoboTricklerUI.ino`, `startup.ino`, `compile_options.h`, `build_opt.h` | IDF version, FreeRTOS tasks, watchdogs, heap, HTTP client configuration | Watchdog shutdown is intentional; heap monitor is debug-only. The HTTP-only build setting matches the project decision. |
| `hardware_pins.h`, `stepper.ino` | ESP32 GPIO/strapping, I2S standard TX, DMA, interrupts | Motor electrical safe-state finding above. Pin and underrun behavior need hardware verification. |
| `trickler_control.ino`, `trickler_runtime.ino`, `scale_rs232.ino` | Cross-core critical sections, task blocking, UART input limits, I2S motion calls | Spinlock-protected state is appropriate; shared configuration needs the conditional cross-task audit. Scale framing and calibration are application contracts. |
| `display_task.ino`, `display_driver.ino`, `display_helpers.ino`, `screenshot.ino`, `lv_conf.h` | FreeRTOS task creation/stack sizes, heap allocation, display GPIO/DMA | LVGL has its own mutex and fixed draw buffer. Screenshot is compile-disabled; display and screenshot rendering are LVGL/TFT contracts rather than IDF APIs. |
| `filesystem_mount.ino`, `filesystem_sync.ino`, `sd_storage.ino`, `profile_actions.ino` | SD SPI/FAT, partition storage, synchronization and reset recovery | Early boot recovers interrupted file-sync backups. |
| `ota_update.ino`, `firmware_check.ino` | OTA slots, update validation, HTTP client | Early SD OTA is the intended recovery path. |
| `wifi_connection.ino`, `wifi_qr.ino` | Wi-Fi STA/AP lifecycle, scan/resource release | The setup AP password uses `esp_efuse_mac_get_default()` and is an accepted project choice. Wi-Fi QR drawing uses the bundled QR component and LVGL, outside core IDF API requirements. |
| `web_server.ino`, `web_api.ino`, `web_file_editor.ino` | HTTP server, static files, update streaming, heap | Static file downloads and unauthenticated routes are intentional. Asynchronous Wi-Fi scan releases results with `WiFi.scanDelete()`. |
| `ui.c`, `ui_Screen1.c`, `ui.h`, `ui_events.h`, `ui_fonts.h`, `ui_events.ino`, `ui_dialogs.ino`, `ui_text.ino` | LVGL calls, UI/task synchronization, filesystem-backed language loading | Generated UI and widget settings are LVGL contracts; no direct IDF API deviation found. Language loading uses the filesystem guard. `volatile` dialog flags remain part of the cross-task review noted above. |
| `tools/partition_layout.py`, `tools/filesystem_build_littlefs_image.py`, `tools/filesystem_stage_littlefs_data.py`, `tools/filesystem_generate_sd_trees.py`, `tools/firmware_build_upload.py`, `tools/release_update_usb_flash.py` | Partition offsets and sizes, LittleFS image construction, OTA/serial release workflow | Generated image is exactly the `0x180000` data-partition size; build uses the selected 8 MB partition layout. |
| `tools/create_profiles.py`, `tools/manual_generate_pdf.py`, `tools/tools.py`; `SD-Files` HTML/JS/CSS/JSON and generated `SD-Files-Gz`/`SD-Files-LittleFS` trees | Firmware route/JSON contracts, data generation, web update and configuration flows | Profile editor and generator now cap RPM; firmware validates RPM against configured steps per revolution. These tools/assets do not call IDF APIs directly. |

## Review and verification status

- Confirmed the installed `esp32-libs/3.3.11/include/esp_common/include/esp_idf_version.h` declares IDF 5.5.5, checked its `sdkconfig` for rollback, security and I2S settings, and checked `default_8MB.csv` against the sketch's OTA/data partition lookups.
- Checked the installed Arduino core and library examples, plus the supplied IDF I2S, watchdog, OTA, Wi-Fi, filesystem, FreeRTOS, HTTP, GPIO, and heap guides against the relevant modules.
- All local documentation links in this report resolve. The project instruction's `tools/compile_upload.py` is absent, so the current equivalent `python tools/firmware_build_upload.py --cli --error --compile-only` was run successfully after the code changes. The generated SD web assets were rebuilt. The profile generator RPM boundary and SD JSON parsing checks also passed. Device behavior remains unverified.

## Verification after implementation

After implementing changes, compile with the current script: `python tools/firmware_build_upload.py --cli --error --compile-only`. Test on hardware: I2S output at boot, feeder starvation, failed I2S write, stop during motion, OTA power loss, LittleFS update with an open file, malformed motor settings, and recovery after a failed update. A compile pass alone cannot verify electrical motor safety or SD recovery behavior.
