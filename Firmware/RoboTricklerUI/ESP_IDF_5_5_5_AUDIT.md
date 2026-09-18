# ESP-IDF 5.5.5 firmware audit

Audit date: 2026-09-18

## Scope and baseline

This review compares the firmware with the locally installed ESP-IDF Programming Guide at
`../../Doc/esp-idf-en-v5.5.5` and with the APIs/examples shipped in Espressif Arduino-ESP32
core 3.3.11.

The comparison is exact enough to be useful: the installed core's SDK configuration identifies
itself as **ESP-IDF 5.5.5**, not merely another release from the 5.5 branch. The application also
builds successfully against that core.

Reviewed areas:

- task ownership, FreeRTOS synchronization, and cross-core state;
- I2S step generation and driver lifecycle;
- filesystem access and recovery;
- Wi-Fi, HTTP management, and provisioning;
- web and SD firmware/filesystem updates;
- memory use, task stacks, LVGL integration, and failure handling;
- the selected partition layout and relevant generated `sdkconfig` settings.

This is a static review plus a clean compile, not a hardware, fault-injection, penetration, or
long-duration soak test.

## Executive summary

The code is already substantially more disciplined than a typical Arduino sketch. In particular,
it uses the current ESP-IDF I2S channel API, bounded fixed buffers in important hot paths, filtered
streaming JSON parsing, explicit LVGL and filesystem mutexes, asynchronous Wi-Fi scanning, atomic
temporary-file replacement, and an OTA-capable two-slot partition table. The current build uses
1,547,208 bytes (46%) of the application partition and 100,408 bytes (30%) of static RAM, leaving
227,272 bytes before runtime allocation.

It is not yet a state-of-the-art networked appliance. The largest actionable gaps are incomplete
cross-core/filesystem ownership and coupling of long HTTP work to LVGL servicing. These are
architectural issues; increasing LVGL memory or enabling more widgets would not help and is not
recommended.

Watchdogs are an explicit project exception: **keep the current watchdogs disabled**. This audit
does not propose enabling them. The consequence is that deadlocks and non-yielding tasks must be
found by diagnostics and soak testing rather than automatic watchdog recovery.

HTTP authentication is also an explicit project exception: **keep the HTTP interface open without
authentication**. The compatible security model is therefore a trusted local network plus strict
request validation. Anyone who can reach the device must be assumed able to use its HTTP controls.
Do not expose or port-forward the service to an untrusted network.

Update authenticity is a third explicit project exception: **keep accepting unsigned firmware and
LittleFS images**. The update path should still reject empty, malformed, truncated, oversized, or
wrong-target input and recover cleanly after interruption. Without signatures or Secure Boot, any
client that can reach the update route—and anybody who can replace files on the SD card—can install
a replacement image. This is accepted rather than an action item.

## Priority findings

### P1 — Constrain the open HTTP control plane

Evidence: `web_server.ino:85-145` registers file listing/editing, reboot, motor start/stop, profile,
target, screenshot, and firmware update routes on plain HTTP port 80 (`RoboTricklerUI.ino:154`).
Several state-changing routes accept an unspecified HTTP method.

Impact: by design, any client able to reach the device can start or stop a motor, reboot it, replace
files, change Wi-Fi credentials, or submit an update. Network isolation is the access boundary. A
browser on the same network can also trigger requests indirectly unless cross-origin requests are
rejected.

Hardening compatible with the no-authentication requirement:

1. Give every route an explicit method. Use `POST`, `PUT`, or `DELETE` for mutations; never mutate on
   `GET`.
2. Make the Wi-Fi provisioning save route available only while deliberate setup AP mode is active.
3. Reject cross-origin browser mutations using `Origin`/`Host` checks and do not enable permissive
   CORS. This does not add a login or credential.
4. Refuse update/editor/reboot routes while trickling, not only selected profile operations.
5. Add request-size limits and reject malformed or duplicate fields before allocating large
   `String` objects.
6. Document that the service is trusted-LAN-only and ensure routers do not publish it through port
   forwarding, UPnP, or an Internet-facing reverse proxy.

HTTPS is optional on a trusted LAN and has a real heap cost on this device. It would protect traffic
in transit but would not restrict who can operate the device.

Relevant documentation: [ESP-IDF security overview](../../Doc/esp-idf-en-v5.5.5/security/security.html)
and [Wi-Fi security](../../Doc/esp-idf-en-v5.5.5/api-guides/wifi-security.html).

### P1 — Give the filesystem one complete ownership model

Evidence: the project has a recursive filesystem mutex, but several operations are outside it:

- `web_file_editor.ino:117` opens a path before acquiring the guard; the guard at lines 127-134 is
  released before `exists()`, `open()`, and the complete `streamFile()` operation at lines 135-161.
- `printDirectory()` at `web_file_editor.ino:371-425` performs a complete directory traversal
  without a guard.
- `loadWebLang()` at `web_api.ino:93-131` and `loadLanguage()` at `ui_text.ino:212-267` access the
  active filesystem without a guard.
- `finishFilesystemSyncConfirm()` calls a multi-file SD/LittleFS copy at
  `filesystem_sync.ino:198-230` without acquiring the filesystem mutex.

These calls can run on the display/web task on core 0 while profile/config work runs in the Arduino
loop task on core 1. A small guarded recovery step does not protect a later open stream.

Recommended implementation: make every logical operation hold the same lock from the first
`exists/open` through the final `close`, or preferably make one storage worker task own SD and
LittleFS and accept bounded commands through a queue. A worker avoids recursive-lock reasoning and
lets callers receive `busy`, `success`, or a precise error without racing. Keep the present
non-blocking UI behavior by using bounded waits rather than `portMAX_DELAY` from UI callbacks.

Also normalize all editor paths centrally and reject empty paths, missing leading `/`, `.`/`..`
segments, backslashes, control characters, and paths longer than the filesystem limit before any
filesystem API call.

Relevant documentation: [SPI master thread safety](../../Doc/esp-idf-en-v5.5.5/api-reference/peripherals/spi_master.html#thread-safety)
and [FreeRTOS additions](../../Doc/esp-idf-en-v5.5.5/api-reference/system/freertos_additions.html).

### P1 — Serialize cross-core application state

Evidence: the LVGL/web task runs on core 0 and `loop()` runs on core 1. `tricklerState` and the
profile-tune request have explicit critical sections, but the larger `config` object, `profileList`,
`weight`, dirty flags, Wi-Fi/server flags, and filesystem selection are read or written from both
tasks without a single ownership rule. Examples include `handleSetTarget()`/`handleSetProfile()` in
`web_api.ino`, LVGL event callbacks in `ui_events.ino`, and the live reads throughout
`trickler_runtime.ino`.

`isTricklerRunning()` before a mutation reduces risk but is a check-then-act sequence, not a
transaction. `volatile` would not fix this; it provides neither atomic compound operations nor
inter-core ownership.

Recommended implementation: make the Arduino loop task the owner of trickler state and active
`Config`. Send typed commands from web/LVGL through a small fixed-length FreeRTOS queue, and return
results through task notifications or a response queue. Publish small read-only status snapshots
under one short critical section. This prevents partially updated arrays/strings and gives all
start/stop/profile/target operations a deterministic order with modest fixed heap cost.

Relevant documentation: [ESP-IDF FreeRTOS SMP](../../Doc/esp-idf-en-v5.5.5/api-guides/freertos-smp.html)
and [FreeRTOS API](../../Doc/esp-idf-en-v5.5.5/api-reference/system/freertos_idf.html).

### Accepted feature — Early firmware acceptance and SD recovery

The generated SDK configuration enables application rollback and the partition table has
`otadata`, `ota_0`, and `ota_1`. Arduino core 3.3.11's default weak `verifyOta()` returns `true`
during `initArduino()`, before sketch `setup()` runs. This immediate acceptance is intentional and
must not be replaced with deferred hardware/configuration validation.

The sketch then initializes the display and mounts storage before calling `initUpdate()`. Crucially,
the SD updater runs before stepper initialization and before configuration, profiles, scale UART,
or Wi-Fi are loaded. A replacement `/firmware.bin` can therefore recover a release even when a bad
configuration or profile would prevent the remainder of normal startup.

Keep this ordering explicit in `startup.ino` and protect it with a regression test. The unavoidable
limit is that the currently running firmware must still reach filesystem initialization and
`initUpdate()`; an image that cannot boot that far requires serial/USB recovery.

Relevant documentation: [App rollback](../../Doc/esp-idf-en-v5.5.5/api-reference/system/ota.html#app-rollback).

### P1 — Separate long HTTP work from LVGL timing

`lvglDisplayTask()` calls both `lv_timer_handler()` and `server.handleClient()`
(`display_task.ino:18-40`). The asynchronous Wi-Fi scan correctly avoids one known stall, but file
streaming, directory listing, filesystem upload, update flashing, JSON parsing, and slow clients can
still keep LVGL from being serviced for visible periods.

Recommended implementation: move all `WebServer` ownership and `handleClient()` calls to one
dedicated network task. That task must not call LVGL directly; it should submit fixed-size UI/status
messages to the display task. Measure both task stack high-water marks and largest free internal
heap block during upload and editor stress before choosing final stack sizes.

If another task is not acceptable, establish a hard latency budget and chunk every response/write
so `lv_timer_handler()` is called within that budget. A dedicated owner is easier to reason about.

### P2 — Remove inappropriate `IRAM_ATTR`

`lvglDisplayTask()` and `initDisplayTask()` are marked `IRAM_ATTR` (`display_task.ino:18,43`) even
though they are normal task/startup functions, call flash-resident code, allocate, lock mutexes, and
use Wi-Fi/LVGL. They are not interrupt handlers and cannot safely execute with the flash cache off
merely because the top-level function is placed in IRAM.

Remove these attributes. Reserve IRAM for callbacks that genuinely execute in an ISR/cache-disabled
context, and then ensure their entire call graph and accessed constants/data are IRAM/DRAM safe.
This saves scarce internal instruction RAM and makes the execution contract clear.

### P2 — Strengthen validation and subsystem health reporting

- Replace permissive `String::toFloat()`/`toInt()` request parsing with full-string parsing using
  `strtof`/`strtol`, checking the end pointer, range, and `isfinite()`.
- Make stepper initialization return a status to startup. If I2S channel creation, preload, enable,
  mutex allocation, or feeder-task creation fails, expose a persistent fault and refuse motor
  starts.
- Check truncation from every important `snprintf`/`strlcpy`, particularly paths and translated
  status text. A truncated path must fail rather than address a different file.
- Add explicit maximum upload sizes before writing. `UPDATE_SIZE_UNKNOWN` should still be bounded by
  the selected target partition and a product-level policy.
- Avoid reporting unnecessary parser internals and filesystem implementation details in production
  error responses.

## Practices that should be kept

- `driver/i2s_std.h`, `i2s_new_channel()`, `i2s_channel_init_std_mode()`, preload, enable, and
  blocking channel writes match the current ESP-IDF I2S state model. The code also checks returned
  byte counts and cleans up failed initialization.
- The I2S stream owner plus motor-run generation ID is a strong design for ordered output and prompt
  cancellation without doing complex work in an ISR.
- Hardware mapping is centralized in `hardware_pins.h`.
- LVGL access is wrapped by one recursive mutex, and dialog objects are created lazily/deleted to
  conserve the LVGL pool.
- Fixed profile buffers, stack formatting buffers, filtered JSON parsing, streamed HTTP responses,
  and debug heap/largest-block metrics all support the project's low-fragmentation goal.
- Temporary file + rename/backup recovery is materially safer than overwriting configuration and
  profiles in place.
- Wi-Fi scanning is asynchronous, reconnect work is rate-limited, and blocking profile-tune motor
  tests are kept out of the LVGL callback.
- `WiFi.persistent(false)` avoids unintended flash writes.
- No recommendation in this report requires enabling LVGL widgets or increasing `LV_MEM_SIZE`.

## Recommended delivery order

1. **HTTP safety release:** require explicit methods, reject cross-origin mutations, restrict the
   Wi-Fi save route to setup mode, validate paths, limit request sizes, and block destructive
   actions while running. Keep the interface credential-free as required.
2. **Ownership release:** introduce typed command queues for trickler/config mutations and complete
   filesystem ownership. Add concurrency stress tests.
3. **Responsiveness release:** move the web server to its own owner task and communicate with LVGL
   and trickler tasks through bounded queues.

## Verification performed

- All four JSON files under `SD-Files` parsed successfully with Python's strict JSON parser.
- `python tools/firmware_build_upload.py --cli --error --compile-only` completed successfully with
  Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5.
- Firmware: 1.48 MiB of 3.19 MiB (46.3%).
- Static RAM: 100,408 of 327,680 bytes (30.6%), leaving 227,272 bytes before runtime allocation.
- LittleFS payload: about 477.7 KiB of 1.50 MiB.
- The linker emitted an executable-stack-note warning for toolchain object `_fixdfdi.o`; this comes
  from the shipped toolchain object, not a project source file. Track it on core/toolchain upgrades,
  but it is not actionable in this sketch.

## Tests to add before calling the firmware production-ready

- Confirm supported local HTTP requests work without credentials. Verify every mutation rejects the
  wrong HTTP method, malformed/oversized data, and a cross-origin browser request.
- Feed valid, empty, malformed, truncated, oversized, wrong-target, and interrupted images through
  both web and SD update paths. Unsigned valid images are expected to remain accepted.
- Put a replacement `/firmware.bin` beside a deliberately corrupt configuration/profile and prove
  the SD update completes before those files are parsed. Also verify immediate OTA acceptance stays
  enabled, matching the intended recovery policy.
- Concurrently stream/list/upload files while selecting profiles, saving a target, synchronizing
  filesystems, and starting/stopping. Run with heap poisoning and stack canaries already supplied by
  the current SDK configuration.
- Brown out or reset during configuration/profile replacement and during each OTA phase.
- Soak Wi-Fi reconnect, display redraw, scale input, and motor runs while logging minimum free heap,
  largest free block, LVGL fragmentation, and per-task stack high-water marks.
- Because watchdogs remain disabled, add an external test harness heartbeat and fail a soak test if
  UI, scale processing, motor control, or HTTP progress stops for longer than its defined deadline.
