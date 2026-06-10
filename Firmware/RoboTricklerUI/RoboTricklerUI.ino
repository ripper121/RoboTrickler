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
#include <freertos/semphr.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#define FW_VERSION "2.12"
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
TaskHandle_t lv_disp_tcb = NULL;
SemaphoreHandle_t lvglMutex = NULL;
#define LV_DRAW_BUF_ROWS 10
static lv_color_t buf[LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS];
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
Config config; // <- global configuration object

bool WIFI_AKTIVE = false;
bool WEB_SERVER_ACTIVE = false;
bool WEB_SERVER_ROUTES_REGISTERED = false;
WebServer server(80);
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = 10000;

#define MOTOR_REV_STEPS 200

#define MAX_TARGET_WEIGHT 999
#define DEC_PLACES 3

float weight = -1.0;
const float EPSILON = 0.00001; // Define a small epsilon value
int decPlaces = DEC_PLACES;
String unit = "";
float lastWeight = 0;
float addWeight = 0.1;
int weightCounter = 0;
int measurementCount = 0;
bool newData = false;
bool running = false;
bool finished = false;
bool firstThrow = true;
int trickleCounter = 0;
bool restart_now = false;
bool calibrationProfilePromptPending = false;
// Track prompt start and fresh scale reads so stale weights are ignored.
unsigned long calibrationProfilePromptTime = 0;
unsigned long lastScaleWeightReadTime = 0;

static bool canStartFirstThrowAtCurrentWeight()
{
  if (config.profile_startAtZero)
  {
    return fabs(weight) <= EPSILON;
  }
  return weight >= 0.0000;
}

String infoMessagBuff[14];
String profileListBuff[32];
byte profileListCount;

static long calculateStepperStepsForUnits(double remainingUnits, double unitsPerThrow, double *outUnits)
{
  if (outUnits != NULL)
  {
    *outUnits = 0.0;
  }
  if ((remainingUnits <= 0.0) || (unitsPerThrow <= 0.0))
  {
    return 0;
  }

  double exactSteps = remainingUnits * ((double)MOTOR_REV_STEPS / unitsPerThrow);
  if ((exactSteps <= 0.0) || (exactSteps > 2147483647.0))
  {
    return 0;
  }

  long steps = (long)exactSteps;
  if (outUnits != NULL)
  {
    *outUnits = ((double)steps * unitsPerThrow) / (double)MOTOR_REV_STEPS;
  }
  return steps;
}

static bool runBulkStepperMove(String &infoText)
{
  double remainingUnits = (double)config.targetWeight - (double)weight - (double)config.profile_weightGap;
  if (remainingUnits <= 0.0)
  {
    return true;
  }

  int stepperNum = config.profile_bulkActuator;
  if ((stepperNum < 1) || (stepperNum > 2))
  {
    stepperNum = 1;
  }

  if (!config.profile_stepperEnabled[stepperNum])
  {
    return true;
  }

  int speed = config.profile_stepperUnitsPerThrowSpeed[stepperNum];
  if (speed <= 0)
  {
    speed = 200;
  }

  double units = 0.0;
  long stepsToMove = calculateStepperStepsForUnits(remainingUnits, config.profile_stepperUnitsPerThrow[stepperNum], &units);
  if (stepsToMove <= 0)
  {
    return true;
  }

  setStepperSpeed(stepperNum, speed);
  step(stepperNum, stepsToMove);
  remainingUnits -= units;
  if (remainingUnits < 0.0)
  {
    remainingUnits = 0.0;
  }

  infoText += "B";
  infoText += String(stepperNum);
  infoText += " ST";
  infoText += String(stepsToMove);
  infoText += " ";

  return true;
}

bool lvglLock()
{
  if (lvglMutex == NULL)
  {
    return true;
  }
  return xSemaphoreTakeRecursive(lvglMutex, portMAX_DELAY) == pdTRUE;
}

void lvglUnlock()
{
  if (lvglMutex != NULL)
  {
    xSemaphoreGiveRecursive(lvglMutex);
  }
}

void setLabelText(lv_obj_t *label, const char *text)
{
  if (lvglLock())
  {
    lv_label_set_text(label, text);
    lvglUnlock();
  }
}

void setWeightLabel(lv_obj_t *label)
{
  char text[32];
  int decimals = decPlaces;
  if (decimals < 0)
  {
    decimals = 0;
  }
  else if (decimals > 6)
  {
    decimals = 6;
  }
  snprintf(text, sizeof(text), "%.*f%s", decimals, weight, unit.c_str());
  setLabelText(label, text);
}

void setObjBgColor(lv_obj_t *obj, uint32_t colorHex)
{
  if (lvglLock())
  {
    lv_obj_set_style_bg_color(obj, lv_color_hex(colorHex), LV_PART_MAIN);
    lvglUnlock();
  }
}

void updateDisplayLog(String logOutput, bool noLog = false)
{
  logOutput += "\n";
  if (!noLog)
  {
    insertLine(logOutput);
    String temp = "";
    for (int i = 0; i < (sizeof(infoMessagBuff) / sizeof(infoMessagBuff[0])); i++)
    {
      temp += infoMessagBuff[i];
    }
    setLabelText(ui_LabelLog, temp.c_str());
  }else
  {
    setLabelText(ui_LabelInfo, logOutput.c_str());    
  }
  DEBUG_PRINT(logOutput);
}

void startCalibrationProfilePrompt()
{
  stopTrickler();
  calibrationProfilePromptPending = true;
  calibrationProfilePromptTime = millis();
  measurementCount = config.profile_generalMeasurements;
  weightCounter = 0;
  newData = false;
}

void handleCalibrationProfilePrompt()
{
  if (!calibrationProfilePromptPending)
  {
    return;
  }

  readWeight();

  if (newData && (lastScaleWeightReadTime > calibrationProfilePromptTime))
  {
    calibrationProfilePromptPending = false;
    newData = false;
    weightCounter = 0;
    if (confirmBox(String(langText("msg_create_profile_prompt")) + String(weight, 3) + " gn", &lv_font_montserrat_14, lv_color_hex(0xFFFFFF)))
    {
      String profileName = "";
      if (createProfileFromCalibration(weight, profileName))
      {
        messageBox(String(langText("msg_profile_created")) + profileName + ".txt", &lv_font_montserrat_14, lv_color_hex(0x00FF00), true);
      }
      else
      {
        messageBox(langText("msg_create_profile_failed"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
      }
    }
  }
}

void setup()
{
  initSetup();
}

void loop()
{
  static uint32_t writeETime = millis();
  static uint32_t readWeightTime = millis();

  if (millis() - writeETime >= 1000)
  {
    writeETime = millis();

#if DEBUG
    char temp[300];
    snprintf(temp, sizeof(temp), "Heap: Free:%lu, Min:%lu, Size:%lu, Alloc:%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getHeapSize(), (unsigned long)ESP.getMaxAllocHeap());
    DEBUG_PRINTLN(temp);

    printf("lv_disp_tcb stackHWM: %d / %d\n", (DISP_TASK_STACK - uxTaskGetStackHighWaterMark(lv_disp_tcb)), DISP_TASK_STACK);
    printf("loop stackHWM: %d / %d\n", (getArduinoLoopTaskStackSize() - uxTaskGetStackHighWaterMark(NULL)), getArduinoLoopTaskStackSize());
#endif
  }

  if (running)
  {
    readWeight();

    if (newData)
    {
      newData = false;
      weightCounter = 0;

      if (weight <= 0.0000)
      {
        firstThrow = true;
        measurementCount = config.profile_generalMeasurements;
      }

      float tolerance = config.profile_tolerance;
      float alarmThreshold = config.profile_alarmThreshold;
      bool calibrationProfile = strcmp(config.profile, "calibrate") == 0;

      DEBUG_PRINT("Weight: ");
      DEBUG_PRINTLN(weight);
      DEBUG_PRINT("TargetWeight: ");
      DEBUG_PRINTLN(String((config.targetWeight - tolerance), 5));
      DEBUG_PRINTLN(String((config.targetWeight + tolerance), 5));
      DEBUG_PRINT("profile_alarmThreshold: ");
      DEBUG_PRINTLN(String((config.targetWeight + alarmThreshold), 5));

      if (!calibrationProfile && (weight >= (config.targetWeight - tolerance - EPSILON)) && (weight >= 0))
      {
        bool weightWithinTolerance = weight <= (config.targetWeight + tolerance + EPSILON);
        if (weightWithinTolerance)
        {
          setLabelTextColor(ui_LabelTricklerWeight, 0x00FF00);
        }
        else
        {
          setLabelTextColor(ui_LabelTricklerWeight, 0xFFFF00);
        }

        if ((weight >= (config.targetWeight + alarmThreshold + EPSILON)) && (alarmThreshold > 0))
        {
          // Send Alarm
          setLabelTextColor(ui_LabelTricklerWeight, 0xFF0000);
          updateDisplayLog(langText("status_over_trickle"), true);
          beep("done");
          delay(250);
          beep("done");
          delay(250);
          beep("done");
          String messageText = langText("msg_over_trickle");
          if (config.profile_trickleCounter)
          {
            messageText += String(trickleCounter);
          }
          stopTrickler();
          messageBox(messageText.c_str(), &lv_font_montserrat_32, lv_color_hex(0xFF0000), true);
        }

        if (!finished)
        {
          beep("done");
          if (config.trickleCounter)
          {
            config.trickleCount++;
          }
          if (config.profile_trickleCounter && weightWithinTolerance)
          {
            trickleCounter++;
            updateDisplayLog(String(langText("status_done")) + langText("status_count") + String(trickleCounter), true);
          }
          else
          {
            updateDisplayLog(langText("status_done"), true);
          }
        }
        measurementCount = 0;
        finished = true;
      }

      if (!finished)
      {
        if ((weight >= 0.0000) && (!firstThrow || canStartFirstThrowAtCurrentWeight()))
        {
          updateDisplayLog(langText("status_running"), true);
          setLabelTextColor(ui_LabelTricklerWeight, 0xFFFFFF);

          DEBUG_PRINTLN("Profile Running");
          String infoText = "";
          infoText.reserve(48);
          if (firstThrow)
          {
            firstThrow = false;
            if (!runBulkStepperMove(infoText))
            {
              updateDisplayLog(langText("status_bulk_failed"), true);
              stopTrickler();
              return;
            }
            if (infoText.length() > 0)
            {
              if (calibrationProfile)
              {
                startCalibrationProfilePrompt();
                return;
              }
              measurementCount = config.profile_generalMeasurements;
              updateDisplayLog(infoText, true);
              return;
            }
          }

          if (config.profile_count <= 0)
          {
            updateDisplayLog(langText("status_invalid_profile"), true);
            stopTrickler();
            return;
          }

          static int stepperSpeedOld[3] = {0, 0, 0};
          int profileStep = config.profile_count - 1;

          for (int i = 0; i < config.profile_count; i++)
          {
            if ((weight) <= (config.targetWeight - config.profile_weight[i]))
            {
              profileStep = i;
              break;
            }
          }

          byte stepperNum = config.profile_num[profileStep];
          if ((stepperNum >= 1) && (stepperNum <= 2) && (stepperSpeedOld[stepperNum] != config.profile_speed[profileStep]))
          {
            setStepperSpeed(stepperNum, config.profile_speed[profileStep]);
            stepperSpeedOld[stepperNum] = config.profile_speed[profileStep];
          }

          if ((config.profile_num[profileStep] < 1) || (config.profile_num[profileStep] > 2))
          {
            updateDisplayLog(langText("status_invalid_stepper"), true);
            stopTrickler();
            return;
          }

          step(config.profile_num[profileStep], config.profile_steps[profileStep]);

          measurementCount = config.profile_measurements[profileStep];

          char infoLine[64];
          snprintf(infoLine,
                   sizeof(infoLine),
                   "W%.3f ST%ld SP%d M%d",
                   config.profile_weight[profileStep],
                   config.profile_steps[profileStep],
                   config.profile_speed[profileStep],
                   config.profile_measurements[profileStep]);
          infoText = infoLine;

          if (calibrationProfile)
          {
            startCalibrationProfilePrompt();
            return;
          }

          updateDisplayLog(infoText, true);
        }
        else if (config.profile_startAtZero)
        {
          updateDisplayLog(langText("status_waiting_zero"), true);
        }
      }

      if (weight <= 0.0000)
      {
        if (finished)
        {
          updateDisplayLog(langText("status_ready"), true);
          finished = false;
        }
      }
    }
  }
  else
  {
    handleCalibrationProfilePrompt();

    if (millis() - readWeightTime >= 1000)
    {
      readWeightTime = millis();
      updateDisplayLog(langText("status_get_weight"), true);
      readWeight();
    }
  }

  maintainWifiConnection();
}
