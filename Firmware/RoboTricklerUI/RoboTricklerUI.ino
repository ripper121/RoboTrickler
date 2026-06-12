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
#include <Update.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#define FW_VERSION "2.12"
// Internal firmware update check endpoint. Do not mirror this value into SD files.
#define DEFAULT_FW_UPDATE_URL "http://strenuous.dev/roboTrickler/userTracker.php"

/*
Legacy Arduino IDE 1.8.19

Boards-Manager esp32 - Espressif Systems version 3.3.10

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

#define DEBUG 0
#if DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#endif

#define SPLASH_LOGO_PATH "/system/logo.bmp"
#define SPLASH_DISPLAY_MS 3000

#define DISP_TASK_STACK 4096 * 2
#define DISP_TASK_PRO 1
#define DISP_TASK_CORE 0
TaskHandle_t lvDisplayTaskHandle = NULL;
SemaphoreHandle_t lvglMutex = NULL;
#define LV_DRAW_BUF_ROWS 4
#define DISPLAY_DRAW_BUF_SIZE (LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS * (LV_COLOR_DEPTH / 8))
static uint32_t buf[DISPLAY_DRAW_BUF_SIZE / sizeof(uint32_t)];
TFT_eSPI tft = TFT_eSPI(LV_HOR_RES_MAX, LV_VER_RES_MAX); /* TFT instance */

SPIClass *SDspi = NULL;

struct Config
{
  char wifi_ssid[64];
  char wifi_psk[64];
  char IPStatic[16];
  char IPGateway[16];
  char IPSubnet[16];
  char IPDns[16];

  char beeper[16];
  char language[8];
  bool fwCheck;
  bool trickleCounter;
  long trickleCount;
  float targetWeight;
  char scale_protocol[32];
  int scale_baud;
  char scale_customCode[32];
  char profile[32];

  byte profile_num[16];
  float profile_weight[16];
  float profile_tolerance;
  float profile_alarmThreshold;
  float profile_weightGap;
  byte profile_bulkActuator;
  int profile_generalMeasurements;
  bool profile_startAtZero;
  bool profile_trickleCounter;
  double profile_stepperUnitsPerThrow[3];
  int profile_stepperUnitsPerThrowSpeed[3];
  bool profile_stepperEnabled[3];
  int profile_measurements[16];
  long profile_steps[16];
  int profile_speed[16];
  int profile_count;
};
// Single source of truth for SD-backed settings and the active trickling profile.
Config config;

bool wifiActive = false;
bool WEB_SERVER_ACTIVE = false;
bool WEB_SERVER_ROUTES_REGISTERED = false;
WebServer server(80);
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = 10000;

#define MOTOR_REV_STEPS 200 // Number of steps for a full revolution of the stepper motor. This is typically 200 for a 1.8 degree stepper, but may be different for other motors.

#define MAX_TARGET_WEIGHT 999
#define DEC_PLACES 3
#define WEIGHT_SCALE_FACTOR 100000.0f // Scale factor to convert weight to an integer for comparison, based on the number of decimal places (e.g., 10000 for 4 decimal places).
#define IDLE_SCALE_READ_INTERVAL 1000 // Interval for reading the scale weight in idle state (milliseconds).

float weight = -1.0;
int decPlaces = DEC_PLACES;
String unit = "";
float lastWeight = 0;
int weightCounter = 0;
int measurementCount = 0;
int activeProfileStep = -1;
bool newData = false;
// State machine that keeps UI events, scale reads, and motor moves in sync.
enum TricklerState
{
  TRICKLER_IDLE,
  TRICKLER_RUNNING,
  TRICKLER_FINISHED,
  TRICKLER_CALIBRATION_PROMPT
};
TricklerState tricklerState = TRICKLER_IDLE;
bool firstProfileMovePending = true;
int trickleCounter = 0;
bool restart_now = false;
// Track prompt start and fresh scale reads so stale weights are ignored.
unsigned long calibrationProfilePromptTime = 0;
unsigned long lastScaleWeightReadTime = 0;

String profileListBuff[32];
byte profileListCount;
int profileListCounter;


void setup()
{
  initSetup();
}

static void handleTricklerState(uint32_t &readWeightTime)
{
  switch (tricklerState)
  {
    case TRICKLER_IDLE:
      handleIdleWeightRead(readWeightTime);
      break;

    case TRICKLER_RUNNING:
      handleRunningTrickler();
      break;

    case TRICKLER_FINISHED:
      handleRunningTrickler();
      break;

    case TRICKLER_CALIBRATION_PROMPT:
      handleIdleWeightRead(readWeightTime);
      break;
  }
}

void logRuntimeStats()
{
//#if DEBUG
  lv_mem_monitor_t lvglMemory;
  lv_mem_monitor(&lvglMemory);

  printf("heap_free:%lu\theap_min:%lu\theap_largest:%lu\theap_internal:%lu\theap_internal_largest:%lu\theap_dma:%lu\t"
         "lvgl_used:%lu\tlvgl_free:%lu\tlvgl_largest:%lu\tstack_lvgl_used:%lu\tstack_loop_used:%lu\n",
         (unsigned long)ESP.getFreeHeap(),
         (unsigned long)ESP.getMinFreeHeap(),
         (unsigned long)ESP.getMaxAllocHeap(),
         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
         (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA),
         (unsigned long)(lvglMemory.total_size - lvglMemory.free_size),
         (unsigned long)lvglMemory.free_size,
         (unsigned long)lvglMemory.free_biggest_size,
         (unsigned long)(DISP_TASK_STACK - uxTaskGetStackHighWaterMark(lvDisplayTaskHandle)),
         (unsigned long)(getArduinoLoopTaskStackSize() - uxTaskGetStackHighWaterMark(NULL)));
//#endif
}

void loop()
{
  static uint32_t writeETime = millis();
  static uint32_t readWeightTime = millis();

  if (millis() - writeETime >= 1000)
  {
    writeETime = millis();
    logRuntimeStats();    
    maintainWifiConnection();
  }

  handleTricklerState(readWeightTime);
}
