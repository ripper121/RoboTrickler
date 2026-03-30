# AGENTS.md

## Purpose

Mandatory rules for any agent working on this firmware.

**Target:** ESP32 firmware using **ESP-IDF**  
**Chip:** ESP32-WROOM-32E-N8  
**Primary goal:** robust, maintainable, ESP-IDF-aligned firmware with minimal practical **flash**, **IRAM**, and **RAM** usage.

---

## Core rules

1. **Read project documentation first**  
   Review the `DOC` folder before making assumptions or writing code. If `DOC` is more specific or newer than anything else, `DOC` is the source of truth.

2. **Check for an existing ESP-IDF solution before writing new code**  
   Review these paths first:
   - `C:\esp\v6.0\esp-idf\examples`
   - `C:\esp\v6.0\esp-idf\components`

   Reuse existing solutions when possible. If no suitable solution exists, implement new code in the style and structure of ESP-IDF examples and components.

3. **Keep the implementation small and practical**  
   Prefer C. Use C++ only when clearly justified and without unnecessary overhead. Implement only what is needed now.

4. **Build after every relevant code change**  
   Before building, add any new library/component to the relevant `CMakeLists.txt` so it is actually compiled and validated by the build.
   After adding or changing a function, driver, or feature, run a build and fix resulting errors before considering the work complete.
   Command to build `idf.py build`
   On this project in PowerShell, export the ESP-IDF environment first, then build in the same shell:
   `. C:\esp\v6.0\esp-idf\export.ps1; idf.py build`

5. **Report what was checked and what was done**  
   For each relevant change, briefly document:
   - which `DOC` files were checked
   - which ESP-IDF examples/components were checked
   - whether reuse was possible
   - why a new implementation was needed
   - which build step was run
   - which issues were fixed

---

## Standard workflow

Use this order for all relevant implementation work:

1. Review `DOC`
2. Check ESP-IDF examples and components
3. Define the smallest working scope
4. Keep board mapping and hardware assumptions centralized
5. Implement with ESP-IDF style and APIs
6. Add any new component/library dependency to the relevant `CMakeLists.txt`
7. Build
8. Fix compile, link, config, and dependency issues
9. Repeat until clean, or explicitly document the blocker

Do not declare work complete without a build, unless a real external blocker is clearly documented.

---

## Implementation standards

### Architecture
- Keep modules small and focused
- Separate where practical:
  - board / pin mapping
  - bus / peripheral access
  - device drivers
  - application logic
- Keep headers small and stable
- Avoid global state unless technically necessary
- Use `static` for internal symbols and `const` consistently

### ESP-IDF style
- Follow ESP-IDF naming, structure, and initialization style
- Use `esp_err_t` for error handling
- Use standard IDF helpers such as `ESP_RETURN_ON_ERROR`, `ESP_GOTO_ON_ERROR`, and `ESP_LOGx` where appropriate
- Prefer existing IDF APIs over custom abstraction layers

### Resource efficiency
- Minimize **flash**:
  - avoid heavy third-party libraries if IDF is sufficient
  - avoid speculative features
  - keep logging lean
  - include only required components and assets

- Minimize **IRAM**:
  - use ISRs only when necessary
  - keep ISRs short
  - avoid unnecessary `IRAM_ATTR`
  - move work from ISR context into tasks/queues when possible

- Minimize **RAM**:
  - prefer small buffers
  - avoid unnecessary dynamic allocation
  - use large framebuffers only if required
  - prefer partial/page-wise updates for displays when practical

### Tasking and runtime behavior
- Use only the tasks that are actually needed
- Keep stack sizes realistic
- Choose priorities deliberately
- Prefer Queue, Event Group, Notification, or Ringbuffer over ad-hoc global signaling
- Measure memory and runtime impact of significant features instead of guessing

### Logging and debugging
- Use one clear `TAG` per module
- Avoid logs in hot paths and ISRs
- Keep debug logging easy to disable
- Treat warnings that suggest logic, API, or type issues as issues to fix

### Robustness
Always consider:
- invalid parameters
- timeout cases
- bus/peripheral failure
- repeated initialization
- reset / restart / brownout behavior
- missing or defective hardware
- invalid filesystem or storage states

Firmware should fail in a controlled way, not chaotically.

---

## Bring-up guidance

Bring up new hardware or features in small steps:

1. verify bus or base peripheral
2. test the simplest possible access
3. verify identification / register access
4. enable the core function
5. test failure paths
6. add convenience features afterwards

Do not introduce multiple unknown failure sources at once.

---

## Kconfig guidance

Use Kconfig only where it adds real value, for example:
- log levels
- optional features
- timeouts
- polling/task intervals
- buffer sizes
- debug switches

Avoid unnecessary Kconfig complexity.

---

## Device guidance

### Stepper / Servo / Encoder / TTL
- Prefer existing IDF mechanisms and examples
- Use IDF APIs for PWM, timers, GPIO, PCNT, UART, and related functions
- Keep realtime paths small
- Implement only the required encoder functionality

### Stepper implementation note
- For STEP pulse generation on this project, prefer `RMT` over `LEDC`
- Reason:
  - `RMT` is a better fit for exact pulse trains used by stepper drivers such as the A4988
  - `RMT` can generate deterministic hardware-timed high/low STEP pulses without per-step GPIO bit-banging
  - `RMT` maps cleanly to "N steps at frequency X" and matches the ESP-IDF `examples/peripherals/rmt/stepper_motor` example
  - `LEDC` is primarily a continuous PWM peripheral and is not the preferred choice here for finite step pulse trains
- Keep DIR and EN control separate from STEP generation when practical
- On this board, STEP should come from ESP32 hardware, while DIR and EN can be controlled through the AW9523

---

## Definition of done

Work is done only when:
- `DOC` was checked
- ESP-IDF examples/components were checked
- the implementation follows ESP-IDF style
- resource usage was kept reasonable
- pin mapping was verified against documentation
- error paths were handled cleanly
- only required functionality was added
- any new library/component was added to the relevant `CMakeLists.txt` before the build
- a build was run after the change
- build issues introduced by the change were fixed or clearly documented as blocked

---

## Priority order

If requirements conflict, follow this order:

1. `DOC`
2. actual hardware / board reality
3. existing suitable ESP-IDF examples or components
4. this `AGENTS.md`
5. personal implementation preference

---

## Working rule

**Read `DOC`, check existing ESP-IDF solutions, implement the smallest practical solution in ESP-IDF style, then build and fix resulting issues.**
