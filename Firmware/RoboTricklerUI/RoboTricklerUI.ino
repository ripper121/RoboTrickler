#include "pindef.h"
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <freertos/semphr.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "constants.h"
#include "config_types.h"

#define FW_VERSION 2.11
#define DEFAULT_FW_UPDATE_URL "https://strenuous.dev/roboTrickler/userTracker.php"

/*
Legacy Arduino IDE 1.8.19

Boards-Manager esp32 - Espressif Systems version 3.3.8

Tools
Board: "ESP32 Dev Module"
Upload Speed: "921600"
CPU Frequency: "240MHz (WiFi/BT)"
Flash Frequency: "80MHz"
Flash Mode: "DIO"
Flash Size: "4MB (32Mb)"
Partition Scheme: "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
Core Debug Level: "None"
PSRAM: "Disabled"
Arduino Runs On: "Core 1"
Events Run On: "Core 0"
*/


#include <lv_conf.h>
#include <lvgl.h>
#include "ui.h"

#ifdef TOUCH_CS
#undef TOUCH_CS
#endif
#include <TFT_eSPI.h>

#define DEBUG 1
#if DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#endif

#define DISP_TASK_STACK 4096 * 2
#define DISP_TASK_PRO 1
#define DISP_TASK_CORE 0
TaskHandle_t lv_disp_tcb = NULL;
SemaphoreHandle_t lvglMutex = NULL;
#define LV_DRAW_BUF_ROWS 10
static lv_color_t buf[LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS];
TFT_eSPI tft = TFT_eSPI(LV_HOR_RES_MAX, LV_VER_RES_MAX); /* TFT instance */

SPIClass *SDspi = NULL;
Config config;

bool wifiActive = false;
WebServer server(80);
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = WIFI_RECONNECT_INTERVAL_MS;

float weight = 0.0;
int decimalPlaces = 3;
String unit = "";
float lastWeight = 0;
float addWeight = 0.1;
int weightCounter = 0;
int measurementCount = 0;
bool newData = false;
bool running = false;
bool finished = false;
bool firstThrow = true;
bool restart_now = false;
bool calibrationProfilePromptPending = false;
unsigned long calibrationProfilePromptTime = 0;
unsigned long lastScaleWeightReadTime = 0;

String infoMessageBuffer[INFO_MESSAGE_LINES];
String profileListBuff[MAX_PROFILE_LIST_ITEMS];
byte profileListCount;
int profileListCounter;
String tempProfile = "";
float tempTargetWeight = 0.0;

bool lvglLock();
void lvglUnlock();
void setLabelText(lv_obj_t *label, const char *text);
void updateWeightLabel(lv_obj_t *label);
void setObjBgColor(lv_obj_t *obj, uint32_t colorHex);
void setLabelTextColor(lv_obj_t *label, uint32_t colorHex);
void updateDisplayLog(String logOutput, bool noLog = false);
bool runBulkStepperMove(String &infoText);
void readScaleWeight();
void startCalibrationProfilePrompt();
void handleCalibrationProfilePrompt();
void handleFirmwareUpdatePage();
void handleFirmwareUpdatePostResult();
void handleFirmwareUpdateUpload();
void checkFirmwareUpdate();
bool mountSdCard();
void printLoopDiagnostics();
void handleTricklerLoop();
void handleWifiReconnect();
void handleWebServerClient();

void setup()
{
  initApp();
}

void loop()
{
  printLoopDiagnostics();
  handleTricklerLoop();
  handleWifiReconnect();
}
