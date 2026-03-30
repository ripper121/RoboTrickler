Here's the full compliance report against AGENTS.md:

Compliance Report — AGENTS.md vs. Codebase
Overall rating: ~55/100

Critical / High Issues
Severity	Issue	Location
CRITICAL	newData declared as float but assigned true/false (bool semantic mismatch)	config.h:82, main.cpp:36
CRITICAL	26+ global variables — tight coupling, concurrency risk	main.cpp:32-57
HIGH	UART init return codes completely ignored — silent failure if scale init fails	main.cpp:110-112
HIGH	Stack sizes 8KB for LVGL task and HTTP server — likely too small	main.cpp:62-67, webserver.cpp:718
HIGH	malloc() in HTTP request handlers — fragmentation, potential leaks	webserver.cpp:134,178,297
HIGH	Busy-wait loops (while (micros()-start < 2) {}) in stepper enable and STEP pulse path — blocks FreeRTOS	stepper_driver.cpp:67, stepper_driver.cpp:257
HIGH	STEP pulse generated via GPIO bit-bang + busy-wait, not RMT — violates explicit stepper guidance	stepper_driver.cpp:248-260
Medium Issues
Severity	Issue	Location
MEDIUM	No Kconfig.projbuild exists — no way to disable debug logging or configure timeouts/buffer sizes without recompiling	missing
MEDIUM	serialWait() blocks the task for 2 full seconds via delay(1) loop — should use FreeRTOS timeout	helper.cpp:92-98
MEDIUM	fwrite() return value not checked in CSV writer — silent data loss possible	csv.cpp:64-76
MEDIUM	s_stepper_stop_requested volatile bool for inter-task signaling — should be Event Group or Task Notification	stepper.cpp:43
MEDIUM	main.cpp is 506 lines mixing UART, display, PID, weight parsing, and orchestration — module separation not applied	main.cpp
What is Done Well
One TAG per module consistently — display.cpp, stepper.cpp, etc.
FreeRTOS Queues used for stepper requests — stepper.cpp:41,94,162
Event Group used for WiFi state — webserver.cpp:43,702
esp_err_t and ESP_LOGI/W/E used in most modules
Filesystem operations check fopen() return — csv.cpp:33-52
CMakeLists.txt correctly registers all source files and components — main/CMakeLists.txt
_delay_micros() correctly yields to FreeRTOS for long delays then busy-waits only the tail — stepper_driver.cpp:84-108
Top Priorities to Fix
Add Kconfig.projbuild with log levels, timeouts, buffer sizes
Migrate STEP pulse to RMT — matches AGENTS.md stepper guidance and IDF example peripherals/rmt/stepper_motor
Fix UART init error handling — wrap with ESP_ERROR_CHECK or ESP_RETURN_ON_ERROR
Replace s_stepper_stop_requested volatile with xTaskNotify or Event Group
Fix float newData → change to bool
Increase stack sizes or measure actual usage with uxTaskGetStackHighWaterMark()