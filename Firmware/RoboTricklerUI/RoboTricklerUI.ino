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
#include "hardware_pins.h"
#include <FS.h>
#if ENABLE_LITTLEFS
#include <LittleFS.h>
#endif
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
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
#define DISP_TASK_PRIORITY 1
#define DISP_TASK_CORE 0
TaskHandle_t lvDisplayTaskHandle = NULL;
SemaphoreHandle_t lvglMutex = NULL;
//#define LV_DRAW_BUF_ROWS ((LV_VER_RES_MAX + 9) / 10)
#define LV_DRAW_BUF_ROWS 16
#define DISPLAY_DRAW_BUF_SIZE (LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS * (LV_COLOR_DEPTH / 8))
static uint32_t displayDrawBuffer[DISPLAY_DRAW_BUF_SIZE / sizeof(uint32_t)];
TFT_eSPI tft = TFT_eSPI(LV_HOR_RES_MAX, LV_VER_RES_MAX); /* TFT instance */

struct Config
{
  bool wifiEnabled;
  char wifiSsid[64];
  char wifiPsk[64];
  char wifiIpStatic[16];
  char wifiIpGateway[16];
  char wifiIpSubnet[16];
  char wifiIpDns[16];

  char beeper[16];
  char language[8];
  bool fwUpdateCheck;
  bool totalCounterEnable;
  long totalCount;
  float targetWeight;
  char scaleProtocol[32];
  int scaleBaud;
  char scaleCustomCode[32];
  int motorStepsPerRev;
  char profileName[32];

  byte profileStepper[16];
  float profileDiffWeight[16];
  float profileTolerance;
  float profileAlarmThreshold;
  float profileWeightGap;
  byte profileBulkStepper;
  int profileGeneralMeasurements;
  bool profileStartAtZero;
  bool profileSessionCounter;
  double profileStepperWeightPerRev[3];
  int profileStepperRpm[3];
  bool profileStepperEnabled[3];
  int profileMeasurements[16];
  long profileSteps[16];
  int profileRpm[16];
  int profileEntryCount;
};
// Single source of truth for flash-backed settings and the active trickling profile.
Config config;

bool wifiActive = false;
bool wifiSetupApActive = false;
bool webServerActive = false;
bool webServerRoutesRegistered = false;
bool filesystemActive = false;
fs::FS *activeFs = NULL;
bool activeFsIsSd = false;
bool sdMounted = false;
bool littleFsMounted = false;
SPIClass *sdSpi = NULL;
#define ACTIVE_FS (*activeFs)

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

#define DEFAULT_MOTOR_STEPS_PER_REV 200 // Default number of steps for a full revolution of the stepper motor (config.motorStepsPerRev). This is typically 200 for a 1.8 degree stepper, but may be different for other motors.

// Weight domain — the single source of truth for how weights are ranged and
// formatted. Every place that stores, shows, enters, or compares a weight goes
// through these and the weightToString()/formatWeight()/clampWeight() helpers,
// so changing precision or range means editing only this block.
/*
WEIGHT_DECIMALS	epsilon	safe WEIGHT_MAX
4               5e-5    ~512
3               5e-4    ~8192
2               5e-3    ~65536
*/
#define WEIGHT_DECIMALS 4        // display/entry/storage precision (i.e. 0.0000)
#define WEIGHT_MIN 0.0f          // minimum settable weight (0.0000)
#define WEIGHT_MAX 500.0f        // maximum settable weight (500.0000)
// Smallest representable weight step (= 10^-WEIGHT_DECIMALS; keep in sync). Weight
// comparisons are done directly on floats — two values closer than half a step
// would display identically, so WEIGHT_EPSILON is the "equal" tolerance. This is
// all the machinery weight comparisons need; no scale-to-integer conversion.
#define WEIGHT_RESOLUTION 0.0001f
#define WEIGHT_EPSILON (WEIGHT_RESOLUTION * 0.5f)
#define IDLE_SCALE_READ_INTERVAL 1000 // Interval for reading the scale weight in idle state (milliseconds).

// UI increment ladder (decades). Kept independent of the comparison-tolerance
// constants above; the smallest step is a plain literal at display precision.
const float WEIGHT_STEP_SIZES[] = {0.001f, 0.01f, 0.1f, 1.0f, 10.0f};
const byte WEIGHT_STEP_COUNT = sizeof(WEIGHT_STEP_SIZES) / sizeof(WEIGHT_STEP_SIZES[0]);

float weight = NAN;
int decimalPlaces = WEIGHT_DECIMALS;
String weightUnit = "";
int weightCounter = 0;
int measurementCount = 0;
int activeProfileStep = -1;
bool newWeightData = false;
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
int sessionCount = 0;
bool restartNow = false;
// Track prompt start and fresh scale reads so stale weights are ignored.
unsigned long calibrationProfilePromptTime = 0;
unsigned long lastScaleWeightReadTime = 0;

// Profile names are bounded by config.profileName (char[32]). Keeping the list in a
// fixed BSS buffer instead of String[32] avoids up to 32 small heap allocations
// (and their fragmentation) every time the profile list is rebuilt.
#define PROFILE_LIST_MAX 32
#define PROFILE_NAME_LEN 32
char profileList[PROFILE_LIST_MAX][PROFILE_NAME_LEN];
byte profileListCount;
int selectedProfileIndex;

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
    case TRICKLER_CALIBRATION_PROMPT:
      handleIdleWeightRead(readWeightTime);
      break;

    case TRICKLER_RUNNING:
    case TRICKLER_FINISHED:
      handleRunningTrickler();
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
  static uint32_t lastRuntimeMaintenanceTime = millis();
  static uint32_t readWeightTime = millis();

  if (millis() - lastRuntimeMaintenanceTime >= 1000)
  {
    lastRuntimeMaintenanceTime = millis();
    logRuntimeStats();    
    maintainWifiConnection();
  }

  handleTricklerState(readWeightTime);
}
