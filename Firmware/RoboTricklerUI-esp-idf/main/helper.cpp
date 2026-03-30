#include "config.h"
#include "arduino_compat.h"
#include "display.h"
#include "pindef.h"
#include "stepper_driver.h"
#include "scale.h"
#include "QuickPID.h"
extern QuickPID roboPID;

#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "ui.h"

static const char *TAG = "helper";

// ============================================================
// Forward declarations (defined in other modules / main.cpp)
// ============================================================
extern void serialFlush();
extern void initWebServer();
extern void initUpdate();
extern bool loadConfiguration(const char *filename, Config &cfg);
extern bool readProfile(const char *filename, Config &cfg);
extern void saveConfiguration(const char *filename, const Config &cfg);
extern void getProfileList();
extern StepperDriver stepper1;
extern StepperDriver stepper2;
extern void stopStepperMotion();

// ============================================================
// Temporary state for profile change detection
// ============================================================
static std::string tempProfile = "";
static float       tempTargetWeight = 0.0f;

// ============================================================
// setLabelTextColor
// ============================================================
void setLabelTextColor(lv_obj_t *label, uint32_t colorHex) {
    lv_color_t color = lv_color_hex(colorHex);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
}

// ============================================================
// insertLine — scroll the info log buffer up and append
// ============================================================
void insertLine(const std::string &newLine) {
    for (int i = 0; i < 13; i++) infoMessagBuff[i] = infoMessagBuff[i + 1];
    infoMessagBuff[13] = newLine;
}

// ============================================================
// messageBox — show the overlay panel with a message
// ============================================================
void messageBox(const std::string &message, const lv_font_t *font, lv_color_t color, bool wait) {
    lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelMessages, message.c_str());
    lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    (void)wait;   // In ESP-IDF the panel is dismissed by the button callback
}

// ============================================================
// beep — play tone via stepper beep method
// ============================================================
void beep(const std::string &beepMode) {
    const std::string b(config.beeper);

    if ((b.find("done") != std::string::npos) && (beepMode.find("done") != std::string::npos))
        stepper1.beep(500);
    if ((b.find("button") != std::string::npos) && (beepMode.find("button") != std::string::npos))
        stepper1.beep(100);
    if ((b.find("both") != std::string::npos) && (beepMode.find("done") != std::string::npos))
        stepper1.beep(500);
    if ((b.find("both") != std::string::npos) && (beepMode.find("button") != std::string::npos))
        stepper1.beep(100);
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
// serialWait — wait up to 2 s for scale data to arrive
// (yields to FreeRTOS every 1 ms via vTaskDelay inside delay())
// ============================================================
bool serialWait() {
    for (int i = 0; i < CONFIG_ROBOTRICKLER_SCALE_TIMEOUT_MS; i++) {
        if (serial1_available()) return false;  // data available — no timeout
        delay(1);
    }
    return true;  // timeout
}

// ============================================================
// serialFlush
// ============================================================
void serialFlush() {
    serial1_flush();
}

// ============================================================
// startMeasurment / stopMeasurment
// ============================================================
void startMeasurment() {
    running  = true;
    finished = false;
    firstThrow = true;
    beep("button");
}

void stopMeasurment() {
    stopStepperMotion();
    running  = false;
    finished = true;
    beep("button");
}

// ============================================================
// setProfile
// ============================================================
void setProfile(int num) {
    strlcpy(config.profile, profileListBuff[num].c_str(), sizeof(config.profile));
    lv_label_set_text(ui_LabelProfile, config.profile);
}

// ============================================================
// saveTargetWeight
// ============================================================
void saveTargetWeight(float w) {
    config.targetWeight = w;
    lv_label_set_text(ui_LabelTarget, str_float(config.targetWeight, 3).c_str());
    saveConfiguration("/config.txt", config);
}

// ============================================================
// corruptProfile — rename and fall back to calibrate
// ============================================================
static void corruptProfile(const std::string &profile_filename) {
    messageBox("Profile Corrupted / Not Found:\n\n" + profile_filename +
               "\n\nCalibration Profile Loaded.",
               &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);

    std::string corrupted = str_replace(profile_filename, ".txt", ".cor.txt");
    // prefix with SD mount point
    std::string full_old = SD_MOUNT_POINT + profile_filename;
    std::string full_new = SD_MOUNT_POINT + corrupted;
    rename(full_old.c_str(), full_new.c_str());

    strlcpy(config.profile, "calibrate", sizeof(config.profile));
    saveConfiguration("/config.txt", config);
    delay(100);
    restart_now = true;
}

// ============================================================
// startTrickler
// ============================================================
void startTrickler() {
    strlcpy(config.mode, "trickler", sizeof(config.mode));
    lv_label_set_text(ui_LabelTricklerStart, "Stop");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

    if (tempProfile != std::string(config.profile)) {
        std::string profile_filename = "/" + std::string(config.profile) + ".txt";
        if (!readProfile(profile_filename.c_str(), config)) {
            corruptProfile(profile_filename);
            return;
        }
    }
    updateDisplayLog("Profile: " + std::string(config.profile) + " selected!", false);

    if (tempTargetWeight != config.targetWeight) saveTargetWeight(config.targetWeight);
    tempTargetWeight = config.targetWeight;

    serialFlush();
    startMeasurment();
}

// ============================================================
// stopTrickler
// ============================================================
void stopTrickler() {
    stopMeasurment();
    serialFlush();
    lv_label_set_text(ui_LabelTricklerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelTricklerWeight, "-.-");
    lv_label_set_text(ui_LabelInfo, "");
    lv_label_set_text(ui_LabelLoggerInfo, "");
}

// ============================================================
// initSetup — runs once from app_main (replaces Arduino setup())
// ============================================================
void initSetup() {
    // Disable RTC WDT (was done in Arduino via rtc_wdt_protect_off/disable)
    // ESP-IDF: simply don't register a RTC WDT handler

    display_init();
    ui_init();
    lv_obj_clear_flag(lv_tabview_get_content(ui_TabView), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(lv_tabview_get_content(ui_TabView), LV_DIR_NONE);

    updateDisplayLog("Robo-Trickler v" + str_float(FW_VERSION, 2) + " // strenuous.dev", false);

    // Initialise stepper (defined in stepper.cpp)
    extern void initStepper();
    initStepper();

    // Mount SD card (defined in sd_config.cpp)
    extern bool sd_mount();
    if (!sd_mount()) {
        messageBox("Card Mount Failed!\n", &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        restart_now = true;
        return;
    }

    if (!loadConfiguration("/config.txt", config)) {
        updateDisplayLog("config.txt deserializeJson() failed", false);

        // Set defaults
        strlcpy(config.wifi_ssid,       "",          sizeof(config.wifi_ssid));
        strlcpy(config.wifi_psk,        "",          sizeof(config.wifi_psk));
        strlcpy(config.IPStatic,        "",          sizeof(config.IPStatic));
        strlcpy(config.IPGateway,       "",          sizeof(config.IPGateway));
        strlcpy(config.IPSubnet,        "",          sizeof(config.IPSubnet));
        strlcpy(config.IPDns,           "",          sizeof(config.IPDns));
        strlcpy(config.scale_protocol,  "GG",        sizeof(config.scale_protocol));
        strlcpy(config.scale_customCode,"",          sizeof(config.scale_customCode));
        config.scale_baud      = 9600;
        strlcpy(config.profile,"calibrate",          sizeof(config.profile));
        config.log_measurements = 20;
        config.targetWeight    = 40.0f;
        config.microsteps      = 1;
        strlcpy(config.beeper, "done",               sizeof(config.beeper));
        config.debugLog        = false;
        config.fwCheck         = true;

        saveConfiguration("/config.txt", config);
        delay(100);
        messageBox("Config File Corrupted / Not Found!\nDefault Config generated.",
                   &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        restart_now = true;
        return;
    }

    getProfileList();

    std::string profile_filename = "/" + std::string(config.profile) + ".txt";
    if (!readProfile(profile_filename.c_str(), config)) {
        corruptProfile(profile_filename);
        return;
    }

    // Initialise scale UART
    serial1_begin(config.scale_baud, IIC_SCL, IIC_SDA);

    initUpdate();
    initWebServer();

    if (WIFI_AKTIVE) {
        // IP is logged inside initWebServer
    }

    // PID setup
    pid_input = 0;
    roboPID.SetOutputLimits(config.pidStepMin, config.pidStepMax);
    roboPID.SetMode(QuickPID::Control::automatic);

    lv_label_set_text(ui_LabelInfo, ("Robo-Trickler v" + str_float(FW_VERSION, 2) + " // strenuous.dev").c_str());
    lv_label_set_text(ui_LabelTarget, str_float(config.targetWeight, 3).c_str());
    lv_label_set_text(ui_LabelProfile, config.profile);

    tempTargetWeight = config.targetWeight;

    ESP_LOGI(TAG, "Setup done.");
}
