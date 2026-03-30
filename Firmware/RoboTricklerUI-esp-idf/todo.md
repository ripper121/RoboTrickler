# TODO — AGENTS.md compliance

## Done

- [x] Replace ArduinoJson with `espressif/cjson` (IDF component registry) — `sd_config.cpp`, `CMakeLists.txt`, `idf_component.yml`
- [x] Fix `float newData` → `bool` — `config.h:82`, `main.cpp:36`
- [x] UART init return codes ignored — wrapped with `ESP_ERROR_CHECK` — `main.cpp:110-112`
- [x] Stack sizes: increased to 12 KB via `Kconfig.projbuild`; HWM logging added to both tasks — `main.cpp`, `webserver.cpp`
- [x] `malloc()` in HTTP handlers — replaced with stack-allocated buffers (max 256 B) — `webserver.cpp:134,178,297`
- [x] `s_stepper_stop_requested volatile bool` — replaced with `std::atomic<bool>` — `stepper.cpp:43`
- [x] `fprintf()` return value not checked in CSV writer — fixed — `csv.cpp:73`
- [x] Add `Kconfig.projbuild` — log levels, stack sizes, scale timeout — `main/Kconfig.projbuild`

---

## Not applicable

- **Busy-wait in `enable()` (2 µs)** — Sub-tick hardware settling time for the A4988 enable signal through the shift register. `vTaskDelay` minimum resolution is 1 ms (1000 µs). This wait is unavoidable and harmless.
- **Busy-wait in STEP pulse (1 µs)** — Minimum STEP HIGH time required by the A4988 datasheet. Same reasoning — sub-tick, cannot use FreeRTOS primitives.
- **Migrate STEP pulse to RMT** — STEP signal is routed through the shift register (`I2S_X_STEP_PIN` is a bit index, not a GPIO pin). RMT requires a direct GPIO connection. Requires PCB redesign — out of scope for firmware.
- **`serialWait()` 2 s block** — Already cooperative: `delay(1)` calls `vTaskDelay(pdMS_TO_TICKS(1))`, yielding to the scheduler every 1 ms for the full 2 s timeout. Other tasks run normally during this period.

---

## Open

- [ ] Reduce global variable count (26+) — `main.cpp:32-57`
- [ ] `main.cpp` (506 lines) mixes UART, display, PID, weight parsing — split into focused modules
