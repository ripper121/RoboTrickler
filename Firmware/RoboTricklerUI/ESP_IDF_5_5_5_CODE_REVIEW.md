# ESP-IDF 5.5.5 code review

Reviewed: 2026-09-18. Target: ESP32 Dev Module, Arduino ESP32 core 3.3.11. The installed core's `esp_idf_version.h` reports ESP-IDF 5.5.5, so the local documentation matches the IDF libraries used by this build. This is a source review, not a claim that every Arduino wrapper implements the IDF examples verbatim. In particular, ESP-IDF does not require replacing Arduino `WiFi`, `SD`, `WebServer`, `Update`, or LVGL with native IDF components solely for conformance.

Documentation root: `C:\Users\ripper121\Documents\GitHub\RoboTrickler\Doc\esp-idf-en-v5.5.5`. Links below point to its rendered HTML pages. Source lines are relative to this folder.

Priorities below identify changes recommended for reliable operation with IDF 5.5.5.

**Project decision:** No HTTP authentication, restrictions on file downloads, or encryption are needed. Secure Boot, flash encryption, HTTPS/TLS, and signed images are outside this review's recommended work. The current HTTP file access and update behavior is intentional; the report does not recommend changing it for access control. The setup AP password derived from the factory MAC address is accepted and needs no change. Disabling the watchdog is intentional; this review does not recommend enabling it. Early SD OTA is the intended last recovery path, so an application rollback self-test is not requested.

Review coverage: all 33 sketch/C/header source files in this folder, plus build/release tools and SD-hosted web assets where they touch the firmware's IDF-facing behavior. The report lists only findings and conditional verification items; files with no identified issue are omitted. Focus areas were direct IDF calls, FreeRTOS synchronization, I2S, UART, OTA/partitions, filesystems, Wi-Fi, HTTP endpoints, startup, hardware pins, and heap use. Existing solutions were checked in the installed ESP32 core examples, the local Arduino libraries, and the supplied IDF documentation. UI layout, translation wording, and calibration accuracy require separate product or hardware validation; they are not specified by ESP-IDF.

The compile resolved the project to Arduino ESP32 core libraries at 3.3.11, ArduinoJson 7.4.3, LVGL 9.5.0, and TFT_eSPI 2.5.43. Although a second `SD` library exists under `Documents\Arduino\libraries`, the compiler selected the ESP32 core's `SD` 3.3.11 implementation. The installed `esp_idf_version.h` declares IDF 5.5.5.

## Remaining findings and recommended changes

| Priority | Location | Finding and recommended change | Documentation |
| --- | --- | --- | --- |
| Critical | `stepper.ino:219`, `hardware_pins.h:8` | `auto_clear = true` makes the I2S driver transmit **zero** on a TX underrun. Bit 0 is the active-high motor-disable output; zero enables the motors. An underrun can also change direction and truncate a step pulse. Make the electrical idle/underrun state motor-disabled through the actual board wiring or a separate output gate, then test starvation and reboot on the board. The write-fault latch stops later moves, but software refilling cannot guarantee a safe state if the feeder stops. The current core enables `CONFIG_I2S_ISR_IRAM_SAFE`, which mitigates the guide's specific flash-cache interrupt delay; retest if the build configuration changes. | [I2S DMA and IRAM safety](../../Doc/esp-idf-en-v5.5.5/api-reference/peripherals/i2s.html) |
| Medium | `ui_events.ino:130-140`, `scale_rs232.ino:361-420`, `scale_rs232.ino:424-524` | The LVGL callback runs on the Core 0 display task, while `readWeight()` runs on the Core 1 Arduino loop task. `cycleScaleProtocol_event_cb()` can rewrite the `config.scaleProtocol` character array and call `serialFlush()` while a request/response transaction is comparing that array, transmitting a request, or reading the reply. Per-call UART driver locking does not make this multi-call protocol transaction atomic. Queue the protocol change for the loop task, or protect the protocol value and the complete flush/request/read operation with one mutex; update the button only after the change is committed. | [IDF FreeRTOS SMP](../../Doc/esp-idf-en-v5.5.5/api-reference/system/freertos_idf.html), [UART driver](../../Doc/esp-idf-en-v5.5.5/api-reference/peripherals/uart.html) |

## Conditional changes and checks

1. **Core and build settings:** Verify the compiled partition CSV and I2S Kconfig values for each release. The core's `default_8MB.csv` has two `0x330000` OTA app slots and one `0x180000` SPIFFS-subtype data partition, which is consistent with the runtime partition lookup. The code's firmware limit resolves to the smaller actual partition size. The installed core enables rollback and the I2S IRAM-safe ISR. Arduino's default `verifyOta()` immediately confirms the image. The project relies on its early SD OTA path rather than an application rollback self-test. In `startup.ino`, SD OTA runs before stepper and configuration initialization, but after display and filesystem initialization; those stages must succeed for this recovery path to run. See [partition tables](../../Doc/esp-idf-en-v5.5.5/api-guides/partition-tables.html).
2. **Hardware limits:** Check `hardware_pins.h` against the actual board schematic and the ESP32 strapping/input-only GPIO rules before changing the map. In particular GPIO0, GPIO2, GPIO12, and GPIO15 affect boot strapping; GPIO34-39 are input-only. A software review cannot establish the board's pull resistors or safe motor power-up state. See [ESP32 GPIO and strapping pins](../../Doc/esp-idf-en-v5.5.5/api-reference/peripherals/gpio.html) and the ESP32 hardware reference.

## Files with findings or conditional verification

Files with no identified IDF/core-specific issue are omitted. All 33 firmware source/header files were reviewed; this table retains only actionable findings and items that cannot be closed without hardware or build validation.

| File | IDF/core/library surface checked | Result |
| --- | --- | --- |
| `startup.ino` | Initialization order, task creation failure paths, SD OTA recovery order | **Conditional.** SD OTA is reached before configuration/profile/stepper initialization, but still requires display and filesystem initialization to succeed. This matches the stated recovery design. |
| `hardware_pins.h` | ESP32 GPIO capabilities, boot strapping pins, input-only pins, shared aliases | **Conditional / Finding.** GPIO34-39 are only used as inputs, but GPIO0/2/12/15 are strapping pins and the shift-register disable polarity needs schematic/board validation. See the critical I2S safe-state finding. |
| `stepper.ino` | IDF 5.5 channel-mode I2S API, DMA sizing, preload/write return handling, FreeRTOS mutex/task lifetime | **Finding.** API usage and the 2,048-byte DMA block are valid, but the zero-valued underrun state is not electrically motor-safe. |
| `scale_rs232.ino` | HardwareSerial/UART request-response sequencing, bounded input parsing, loop-task blocking | **Finding.** Buffers and parsing are bounded, but the complete UART transaction is not synchronized with the Core 0 protocol-change callback. |
| `display_driver.ino` | LVGL tick/display/input callbacks, TFT_eSPI DMA setup, task yielding | **Conditional.** The implementation follows the installed LVGL/TFT APIs; panel rotation, touch calibration, and electrical timing require device testing. |
| `ui_events.ino` | Core 0 event callbacks, mutation gates, scale protocol changes | **Finding.** Runtime target/profile mutation gates are present, but the UART protocol change can race the Core 1 scale transaction. |

## Review and verification status

- Confirmed the installed `esp32-libs/3.3.11/include/esp_common/include/esp_idf_version.h` declares IDF 5.5.5, checked its `sdkconfig` for rollback, security and I2S settings, and checked `default_8MB.csv` against the sketch's OTA/data partition lookups.
- Rechecked the installed core's `Update/examples/SD_Update` and `Update/examples/OTAWebUpdater`, `HTTPClient/examples/StreamHttpClient`, `WiFi/examples/WiFiScanAsync`, `WiFi/examples/WiFiClientStaticIP`, and the local LVGL/TFT_eSPI examples; checked the supplied IDF I2S, UART, watchdog, OTA, Wi-Fi, filesystem, FreeRTOS, HTTP, GPIO, and heap guides against the relevant modules. The project uses the installed wrapper APIs.
- All local documentation links in this report resolve. The project instruction's `tools/compile_upload.py` is absent, so the current equivalent `python tools/firmware_build_upload.py --cli --error --compile-only` was run successfully on 2026-09-18: firmware is 1,558,828 bytes (46% of the 3,342,336-byte app slot), static/global RAM is 100,464 bytes (30%), and the staged LittleFS files are 489,331 bytes in the 1,572,864-byte image. The only compiler/linker diagnostic is the bundled toolchain's `_fixdfdi.o` missing `.note.GNU-stack` warning. All JSON files under `SD-Files` parse successfully. Device behavior remains unverified.

## Verification after implementation

After implementing changes, compile with the current script: `python tools/firmware_build_upload.py --cli --error --compile-only`. Test on hardware: I2S output at boot, feeder starvation, failed I2S write, stop during motion, changing the scale protocol during an idle read, OTA power loss, LittleFS update with an open file, malformed motor settings, and recovery after a failed update. A compile pass alone cannot verify electrical motor safety, UART transaction ordering, or SD recovery behavior.
