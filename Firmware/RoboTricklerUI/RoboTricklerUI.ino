/*
Legacy Arduino IDE 1.8.19

Boards-Manager esp32 - Espressif Systems version 3.3.10

Tools
Board: "ESP32 Dev Module"
Upload Speed: "921600"
CPU Frequency: "240MHz (WiFi/BT)"
Flash Frequency: "80MHz"
Flash Mode: "DIO"
Flash Size: "8MB (64Mb)"
Partition Scheme: "8M with spiffs (3MB APP/1.5MB SPIFFS)"
Core Debug Level: "None"
PSRAM: "Disabled"
Arduino Runs On: "Core 1"
Events Run On: "Core 0"
*/

#include "compile_options.h"
#include "pindef.h"
#include <FS.h>
#if ENABLE_LITTLEFS
#include <LittleFS.h>
#endif
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>
#include <rtc_wdt.h>
#include <qrcode.h>
#include <freertos/semphr.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#define FW_VERSION "2.14"
// Internal firmware update check endpoint. Do not mirror this value into SD files.
#define DEFAULT_FW_UPDATE_URL "http://strenuous.dev/roboTrickler/userTracker.php"


#include <lv_conf.h>
#include <lvgl.h>
#include "ui.h"

#ifdef TOUCH_CS
#undef TOUCH_CS
#endif
#include <TFT_eSPI.h>

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
TaskHandle_t lvDisplayTaskHandle = NULL;
SemaphoreHandle_t lvglMutex = NULL;
//#define LV_DRAW_BUF_ROWS ((LV_VER_RES_MAX + 9) / 10)
#define LV_DRAW_BUF_ROWS 16
#define DISPLAY_DRAW_BUF_SIZE (LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS * (LV_COLOR_DEPTH / 8))
static uint32_t buf[DISPLAY_DRAW_BUF_SIZE / sizeof(uint32_t)];
TFT_eSPI tft = TFT_eSPI(LV_HOR_RES_MAX, LV_VER_RES_MAX); /* TFT instance */

struct Config
{
  bool wifi_enabled;
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
  int motorRevSteps;
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
  double profile_stepperUnitsPerRev[3];
  int profile_stepperUnitsPerRevSpeed[3];
  bool profile_stepperEnabled[3];
  int profile_measurements[16];
  long profile_steps[16];
  int profile_speed[16];
  int profile_count;
};
// Single source of truth for flash-backed settings and the active trickling profile.
Config config;

bool wifiActive = false;
bool WIFI_SETUP_AP_ACTIVE = false;
bool WEB_SERVER_ACTIVE = false;
bool WEB_SERVER_ROUTES_REGISTERED = false;
bool FILESYSTEM_ACTIVE = false;
fs::FS *activeFS = NULL;
bool activeFSIsSD = false;
bool sdMounted = false;
bool littleFSMounted = false;
SPIClass *SDspi = NULL;
#define ACTIVE_FS (*activeFS)

enum FilesystemSyncDirection
{
  FILESYSTEM_SYNC_NONE,
  FILESYSTEM_SYNC_FLASH_TO_SD,
  FILESYSTEM_SYNC_SD_TO_FLASH
};

WebServer server(80);
DNSServer dnsServer;
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = 10000;

#define DEFAULT_MOTOR_REV_STEPS 200 // Default number of steps for a full revolution of the stepper motor (config.motorRevSteps). This is typically 200 for a 1.8 degree stepper, but may be different for other motors.

#define MAX_TARGET_WEIGHT 999
#define DEC_PLACES 3
#define WEIGHT_SCALE_FACTOR 100000.0f // Scale factor to convert weight to an integer for comparison, based on the number of decimal places (e.g., 10000 for 4 decimal places).
#define IDLE_SCALE_READ_INTERVAL 1000 // Interval for reading the scale weight in idle state (milliseconds).

const float WEIGHT_STEP_SIZES[] = {0.001, 0.01, 0.1, 1.0, 10.0};
const byte WEIGHT_STEP_COUNT = sizeof(WEIGHT_STEP_SIZES) / sizeof(WEIGHT_STEP_SIZES[0]);

float weight = NAN;
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

// Profile names are bounded by config.profile (char[32]). Keeping the list in a
// fixed BSS buffer instead of String[32] avoids up to 32 small heap allocations
// (and their fragmentation) every time the profile list is rebuilt.
#define PROFILE_LIST_MAX 32
#define PROFILE_NAME_LEN 32
char profileListBuff[PROFILE_LIST_MAX][PROFILE_NAME_LEN];
byte profileListCount;
int profileListCounter;

void disableRuntimeWatchdogs()
{
  disableLoopWDT();
  esp_task_wdt_deinit();
  rtc_wdt_disable();
}

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
#if DEBUG
  lv_mem_monitor_t lvglMemory;
  memset(&lvglMemory, 0, sizeof(lvglMemory));
  if (lvglLock())
  {
    lv_mem_monitor(&lvglMemory);
    lvglUnlock();
  }

  printf("heap_free:%lu\theap_min:%lu\theap_largest:%lu\theap_internal:%lu\theap_internal_largest:%lu\theap_dma:%lu\t"
         "lvgl_used:%lu\tlvgl_free:%lu\tlvgl_largest:%lu\tlvgl_frag:%u\tlvgl_max_used:%lu\t"
         "stack_lvgl_peak_used:%lu\tstack_loop_peak_used:%lu\n",
         (unsigned long)ESP.getFreeHeap(),
         (unsigned long)ESP.getMinFreeHeap(),
         (unsigned long)ESP.getMaxAllocHeap(),
         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
         (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA),
         (unsigned long)(lvglMemory.total_size - lvglMemory.free_size),
         (unsigned long)lvglMemory.free_size,
         (unsigned long)lvglMemory.free_biggest_size,
         (unsigned int)lvglMemory.frag_pct,
         (unsigned long)lvglMemory.max_used,
         (unsigned long)(DISP_TASK_STACK - uxTaskGetStackHighWaterMark(lvDisplayTaskHandle)),
         (unsigned long)(getArduinoLoopTaskStackSize() - uxTaskGetStackHighWaterMark(NULL)));
#endif
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
