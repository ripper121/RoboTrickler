#include "pindef.h"
#include "arduino_compat.h"
#include "config.h"
#include "display.h"
#include "shift_register.h"
#include "stepper_driver.h"

#include <string>
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "nvs_flash.h"

// LVGL UI
#include "ui.h"

// QuickPID
#include "QuickPID.h"

static const char *TAG = "main";

// ============================================================
// Global variable definitions (declared extern in config.h)
// ============================================================
Config config;

float weight       = 0.0f;
float addWeight    = 0.1f;
int   weightCounter  = 0;
int   measurementCount = 0;
bool  newData      = false;
bool  running      = false;
bool  finished     = false;
bool  firstThrow   = true;
int   logCounter   = 1;
std::string path   = "empty";
bool  restart_now  = false;
std::string unit   = "";
int   dec_places   = 3;
float lastWeight   = 0;

bool  WIFI_AKTIVE  = false;
bool  PID_AKTIVE   = false;

std::string infoMessagBuff[14];
std::string profileListBuff[32];
uint8_t     profileListCount = 0;

// PID (non-static so helper.cpp can extern them)
float pid_input  = 0;
float pid_output = 0;
QuickPID roboPID(&pid_input, &pid_output, &config.targetWeight);

// LVGL task handle
static TaskHandle_t lv_disp_tcb = NULL;

#define DISP_TASK_STACK  CONFIG_ROBOTRICKLER_DISP_STACK_SIZE
#define DISP_TASK_CORE   0
#define DISP_TASK_PRIO   1
#define MAIN_LOOP_STACK  CONFIG_ROBOTRICKLER_MAIN_STACK_SIZE
#define MAIN_LOOP_CORE   1
#define MAIN_LOOP_PRIO   1

// UART1 — scale serial
#define SCALE_UART       UART_NUM_1
#define SCALE_BUF_SIZE   256

// ============================================================
// Forward declarations (functions defined in other modules)
// ============================================================
void setStepperSpeed(int num, int speed);
void step(int num, int steps, bool osc, bool rev, bool accel);
void initSetup();
void updateDisplayLog(const std::string &msg, bool noLog = false);
void setLabelTextColor(lv_obj_t *label, uint32_t colorHex);
void startTrickler();
void stopTrickler();
void beep(const std::string &mode);
void serialFlush();
bool serialWait();
void messageBox(const std::string &msg, const lv_font_t *font, lv_color_t color, bool wait);
void insertLine(const std::string &newLine);
std::string ceateCSVFile(const char *folderName, const char *fileName);
void writeCSVFile(const char *path, float w, int count);
void logToFile(const std::string &msg);
bool loadConfiguration(const char *filename, Config &cfg);
bool readProfile(const char *filename, Config &cfg);
void saveConfiguration(const char *filename, const Config &cfg);
void getProfileList();
void initWebServer();
void initUpdate();
void saveTargetWeight(float w);

// ============================================================
// UART1 helpers (Serial1 replacement)
// ============================================================
void serial1_begin(int baud, gpio_num_t rx_pin, gpio_num_t tx_pin) {
    uart_config_t cfg = {};
    cfg.baud_rate  = baud;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_APB;
    ESP_ERROR_CHECK(uart_driver_install(SCALE_UART, SCALE_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(SCALE_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SCALE_UART, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

int serial1_available() {
    size_t len = 0;
    uart_get_buffered_data_len(SCALE_UART, &len);
    return (int)len;
}

// Read bytes until terminator character or buffer full; returns bytes read (excluding terminator)
int serial1_read_until(uint8_t terminator, char *buf, int max_len) {
    int idx = 0;
    while (idx < max_len - 1) {
        uint8_t c;
        int r = uart_read_bytes(SCALE_UART, &c, 1, pdMS_TO_TICKS(10));
        if (r <= 0) break;
        if (c == terminator) break;
        buf[idx++] = (char)c;
    }
    buf[idx] = '\0';
    return idx;
}

void serial1_write(const uint8_t *data, size_t len) {
    uart_write_bytes(SCALE_UART, (const char *)data, len);
}

void serial1_write_str(const char *s) {
    uart_write_bytes(SCALE_UART, s, strlen(s));
}

void serial1_flush() {
    uart_flush(SCALE_UART);
    uart_flush_input(SCALE_UART);
}

// ============================================================
// Weight string parser (identical logic to Arduino version)
// ============================================================
static void stringToWeight(const char *input, float *value, int *decimalPlaces) {
    char filtered[64];
    int j = 0;
    bool dotFound = false;
    int decimals  = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if ((c >= '0' && c <= '9') || c == '.') {
            if (c == '.') dotFound = true;
            else if (dotFound) decimals++;
            filtered[j++] = c;
        }
    }
    filtered[j] = '\0';
    *decimalPlaces = decimals;
    *value = strtof(filtered, nullptr);
}

// ============================================================
// readWeight — replaces Serial1-based weight reading
// ============================================================
static void readWeight() {
    if (serial1_available()) {
        char buff[64] = {};
        serial1_read_until(0x0A, buff, sizeof(buff));

        ESP_LOGD(TAG, "Scale raw: %s", buff);

        if (config.debugLog) {
            lv_label_set_text(ui_LabelTricklerWeight, buff);
            std::string logMsg = std::string(buff) + "\n";
            logToFile(logMsg);
        }

        std::string sbuf(buff);
        size_t dot = sbuf.find('.');

        if (dot != std::string::npos) {
            weight = 0;
            dec_places = 0;
            stringToWeight(buff, &weight, &dec_places);

            if (sbuf.find('-') != std::string::npos) weight = -weight;

            if (sbuf.find('g') != std::string::npos || sbuf.find('G') != std::string::npos) {
                unit = " g";
                if (sbuf.find("gn") != std::string::npos || sbuf.find("GN") != std::string::npos ||
                    sbuf.find("gr") != std::string::npos || sbuf.find("GR") != std::string::npos) {
                    unit = " gn";
                }
            }

            if (lastWeight == weight) {
                if (weightCounter >= measurementCount) {
                    newData = true;
                    weightCounter = 0;
                }
                weightCounter++;
            } else {
                weightCounter = 0;
                if (!config.debugLog) {
                    std::string w_str = str_float(weight, dec_places) + unit;
                    lv_label_set_text(ui_LabelTricklerWeight, w_str.c_str());
                    lv_label_set_text(ui_LabelLoggerWeight,   w_str.c_str());
                }
            }
            lastWeight = weight;
        }

        serialFlush();
    } else {
        bool timeout = false;
        const std::string proto(config.scale_protocol);

        if (proto == "GUG" || proto == "GG") {
            uint8_t cmd[] = {0x1B, 0x70, 0x0D, 0x0A};
            serial1_write(cmd, sizeof(cmd));
            timeout = serialWait();
        } else if (proto == "AD") {
            serial1_write_str("Q\r\n");
            timeout = serialWait();
        } else if (proto == "KERN") {
            serial1_write_str("w");
            timeout = serialWait();
        } else if (proto == "KERN-ABT") {
            serial1_write_str("D05\r\n");
            timeout = serialWait();
        } else if (proto == "SBI") {
            serial1_write_str("P\r\n");
            timeout = serialWait();
        } else if (proto == "CUSTOM") {
            serial1_write_str(config.scale_customCode);
            timeout = serialWait();
        } else {
            timeout = serialWait();
        }

        if (timeout) {
            updateDisplayLog("Timeout! Check RS232 Wiring & Settings!", true);
            delay(500);
            newData = false;
        }
    }
}

// ============================================================
// updateDisplayLog — update LVGL info labels
// ============================================================
void updateDisplayLog(const std::string &logOutput, bool noLog) {
    lv_label_set_text(ui_LabelInfo,       logOutput.c_str());
    lv_label_set_text(ui_LabelLoggerInfo, logOutput.c_str());

    if (!noLog) {
        insertLine(logOutput + "\n");
        std::string temp;
        for (auto &line : infoMessagBuff) temp += line;
        lv_label_set_text(ui_LabelLog, temp.c_str());
    }

    ESP_LOGD(TAG, "%s", logOutput.c_str());
}

// ============================================================
// LVGL task — runs on core 0
// ============================================================
static void lvgl_disp_task(void *) {
    static uint32_t hwm_time = 0;
    while (1) {
        lv_timer_handler();
        if (millis() - hwm_time >= 10000) {
            hwm_time = millis();
            ESP_LOGD(TAG, "lvglTask stack HWM: %u bytes",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================
// Main loop task — runs on core 1, replaces Arduino loop()
// ============================================================
static void main_loop_task(void *) {
    static uint32_t writeETime     = millis();
    static uint32_t readWeightTime = millis();

    while (1) {
        // Periodic heap and stack info (debug)
        if (millis() - writeETime >= 1000) {
            writeETime = millis();
            ESP_LOGD(TAG, "Heap free=%u min=%u stack HWM=%u",
                     (unsigned)ESP_FREE_HEAP(), (unsigned)ESP_MIN_FREE_HEAP(),
                     (unsigned)uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
        }

        if (running) {
            readWeight();

            if (newData) {
                newData = false;
                weightCounter = 0;

                if (weight <= 0.0f) {
                    firstThrow = true;
                    measurementCount = 0;
                }

                if (std::string(config.mode).find("trickler") != std::string::npos) {
                    float tolerance      = config.profile_tolerance;
                    float alarmThreshold = config.profile_alarmThreshold;
                    if (PID_AKTIVE) {
                        tolerance      = config.pidTolerance;
                        alarmThreshold = config.pidAlarmThreshold;
                    }

                    const float EPSILON = 0.00001f;

                    if ((weight >= (config.targetWeight - tolerance - EPSILON)) && weight >= 0) {
                        if (weight <= (config.targetWeight + tolerance + EPSILON)) {
                            setLabelTextColor(ui_LabelTricklerWeight, 0x00FF00);
                        } else {
                            setLabelTextColor(ui_LabelTricklerWeight, 0xFFFF00);
                        }

                        if ((weight >= (config.targetWeight + alarmThreshold + EPSILON)) && alarmThreshold > 0) {
                            setLabelTextColor(ui_LabelTricklerWeight, 0xFF0000);
                            updateDisplayLog("!Over trickle!", true);
                            beep("done"); delay(250);
                            beep("done"); delay(250);
                            beep("done");
                            serialFlush();
                            stopTrickler();
                            messageBox("!Over trickle!\n!Check weight!", &lv_font_montserrat_32, lv_color_hex(0xFF0000), true);
                        }

                        if (!finished) {
                            beep("done");
                            updateDisplayLog("Done :)", true);
                            serialFlush();
                        }
                        measurementCount = 0;
                        finished = true;
                    } else {
                        finished = false;
                    }
                }

                if (!finished) {
                    if (std::string(config.mode).find("trickler") != std::string::npos && weight >= 0) {
                        updateDisplayLog("Running...", true);
                        setLabelTextColor(ui_LabelTricklerWeight, 0xFFFFFF);

                        if (PID_AKTIVE) {
                            std::string infoText;
                            uint8_t stepperNum = 1;
                            static int stepperSpeedOld = 0;

                            float gap = fabsf(config.targetWeight - weight);
                            if (gap <= config.pidThreshold) {
                                roboPID.SetTunings(config.pidConsKp, config.pidConsKi, config.pidConsKd);
                                measurementCount = config.pidConMeasurements;
                                stepperNum = config.pidConsNum;
                                infoText += "GOV:CON ";
                            } else {
                                roboPID.SetTunings(config.pidAggKp, config.pidAggKi, config.pidAggKd);
                                measurementCount = config.pidAggMeasurements;
                                stepperNum = config.pidAggNum;
                                infoText += "GOV:AGG ";
                            }
                            pid_input = weight;
                            roboPID.Compute();

                            long steps = (long)pid_output;
                            infoText += "GAP:" + str_float(gap, 3) + " ";
                            infoText += "STP:" + std::to_string(steps) + " ";
                            infoText += "MES:" + std::to_string(measurementCount);
                            updateDisplayLog(infoText, true);

                            if (stepperSpeedOld != config.pidSpeed)
                                setStepperSpeed(stepperNum, config.pidSpeed);
                            stepperSpeedOld = config.pidSpeed;
                            step(stepperNum, steps, config.pidOscillate, config.pidReverse, config.pidAcceleration);

                        } else {
                            std::string infoText;
                            if (firstThrow && config.profile_stepsPerUnit > 0) {
                                firstThrow = false;
                                double steps = (config.targetWeight - weight) * config.profile_stepsPerUnit;
                                setStepperSpeed(1, config.profile_speed[0]);
                                step(config.profile_num[0], (int)steps,
                                     config.profile_oscillate[0], config.profile_reverse[0], config.profile_acceleration[0]);
                                infoText += "FirstThrow steps:" + str_float((float)steps, 0);
                                serialFlush();
                                vTaskDelay(pdMS_TO_TICKS(1));
                                continue;
                            } else {
                                static int stepperSpeedOld2 = 0;
                                int profileStep = 0;
                                for (int i = 0; i < config.profile_count; i++) {
                                    if (weight <= (config.targetWeight - config.profile_weight[i])) {
                                        profileStep = i;
                                        break;
                                    }
                                }
                                if (stepperSpeedOld2 != config.profile_speed[profileStep])
                                    setStepperSpeed(config.profile_num[profileStep], config.profile_speed[profileStep]);
                                stepperSpeedOld2 = config.profile_speed[profileStep];

                                long ps = config.profile_steps[profileStep];
                                bool osc = config.profile_oscillate[profileStep];
                                if (ps > 360 && osc) {
                                    for (int j = 0; j < (int)(ps / 360); j++)
                                        step(config.profile_num[profileStep], 360, osc, config.profile_reverse[profileStep], config.profile_acceleration[profileStep]);
                                    step(config.profile_num[profileStep], (int)(ps % 360), osc, config.profile_reverse[profileStep], config.profile_acceleration[profileStep]);
                                } else {
                                    step(config.profile_num[profileStep], (int)ps, osc, config.profile_reverse[profileStep], config.profile_acceleration[profileStep]);
                                }

                                measurementCount = config.profile_measurements[profileStep];
                                infoText += "W"  + str_float(config.profile_weight[profileStep], 3) + " ";
                                infoText += "ST" + std::to_string(config.profile_steps[profileStep]) + " ";
                                infoText += "SP" + std::to_string(config.profile_speed[profileStep]) + " ";
                                infoText += "M"  + std::to_string(config.profile_measurements[profileStep]);

                                if (std::string(config.profile) == "calibrate") stopTrickler();
                            }
                            updateDisplayLog(infoText, true);
                        }
                        serialFlush();
                    }

                    if (std::string(config.mode).find("logger") != std::string::npos && weight > 0) {
                        if (path == "empty") {
                            path = ceateCSVFile("/log", "log");
                        }
                        writeCSVFile(path.c_str(), weight, logCounter);
                        measurementCount = config.log_measurements;
                        std::string infoText = "Count: " + std::to_string(logCounter) + " Saved :)";
                        updateDisplayLog(infoText, true);
                        finished = true;
                        beep("done");
                    }
                }

                if (weight <= 0.0f) {
                    if (finished) {
                        if (std::string(config.mode).find("logger") != std::string::npos) {
                            logCounter++;
                            updateDisplayLog("Place next peace...", true);
                            beep("done");
                        } else {
                            updateDisplayLog("Ready", true);
                        }
                        finished = false;
                    }
                }
            }
        } else {
            path = "empty";
            logCounter = 1;
            if (millis() - readWeightTime >= 1000) {
                readWeightTime = millis();
                updateDisplayLog("Get Weight...", true);
                readWeight();
                std::string w_str = str_float(weight, dec_places) + unit;
                lv_label_set_text(ui_LabelTricklerWeight, w_str.c_str());
                lv_label_set_text(ui_LabelLoggerWeight,   w_str.c_str());
            }
        }

        // WiFi reconnect watchdog (handled in wifi module, but check here too)
        // (initWebServer sets up the event-based reconnect)

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============================================================
// app_main — replaces setup() + loop()
// ============================================================
extern "C" void app_main(void) {
    // NVS (needed by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    initSetup();

    // Create LVGL display task on core 0
    xTaskCreatePinnedToCore(lvgl_disp_task, "lvglTask",
                            DISP_TASK_STACK, NULL, DISP_TASK_PRIO,
                            &lv_disp_tcb, DISP_TASK_CORE);

    // Create main loop task on core 1
    xTaskCreatePinnedToCore(main_loop_task, "mainLoop",
                            MAIN_LOOP_STACK, NULL, MAIN_LOOP_PRIO,
                            NULL, MAIN_LOOP_CORE);

    // app_main can return — tasks keep running
}
