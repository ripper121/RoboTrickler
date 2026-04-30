#include "pindef.h"
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <A4988.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <rtc_wdt.h>
#include <freertos/semphr.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#define FW_VERSION 2.10

/*
Legacy Arduino IDE 1.8.19

Boards-Manager esp32 - Espressif Systems version 3.1.3

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

// 3 seconds WDT
// #define WDT_TIMEOUT 10

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
/*Change to your screen resolution*/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10];
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
  bool fwCheck;
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
  int profile_generalMeasurements;
  double profile_stepperUnitsPerThrow[3];
  int profile_stepperUnitsPerThrowSpeed[3];
  bool profile_stepperEnabled[3];
  int profile_measurements[16];
  long profile_steps[16];
  int profile_speed[16];
  bool profile_reverse[16];
  int profile_count;
};
Config config; // <- global configuration object

bool WIFI_AKTIVE = false;
WebServer server(80);
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = 10000;

#define MOTOR_STEPS 200
#define ACCEL 1000
A4988 stepper1(MOTOR_STEPS, I2S_X_DIRECTION_PIN, I2S_X_STEP_PIN, I2S_X_DISABLE_PIN);
A4988 stepper2(MOTOR_STEPS, I2S_Y_DIRECTION_PIN, I2S_Y_STEP_PIN, I2S_Y_DISABLE_PIN);

#define MAX_TARGET_WEIGHT 999
float weight = 0.0;
const float EPSILON = 0.00001; // Define a small epsilon value
int dec_places = 3;
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

  double exactSteps = remainingUnits * (200.0 / unitsPerThrow);
  if ((exactSteps <= 0.0) || (exactSteps > 2147483647.0))
  {
    return 0;
  }

  long steps = (long)exactSteps;
  if (outUnits != NULL)
  {
    *outUnits = ((double)steps * unitsPerThrow) / 200.0;
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

  for (int stepperNum = 2; stepperNum >= 1; stepperNum--)
  {
    if (!config.profile_stepperEnabled[stepperNum])
    {
      continue;
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
      continue;
    }

    setStepperSpeed(stepperNum, speed);
    step(stepperNum, stepsToMove, false);
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
  }

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
  int decimals = dec_places;
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
    lv_obj_set_style_bg_color(obj, lv_color_hex(colorHex), LV_PART_MAIN | LV_STATE_DEFAULT);
    lvglUnlock();
  }
}

void updateDisplayLog(String logOutput, bool noLog = false)
{
  setLabelText(ui_LabelInfo, logOutput.c_str());
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
  }

  DEBUG_PRINT(logOutput);
}

// Function to clean the input buffer, return a float, and count decimal places
void stringToWeight(const char *input, float *value, int *decimalPlaces)
{
  char filteredBuffer[64]; // Make sure this is large enough to hold the filtered result
  int j = 0;
  bool dotFound = false;
  int decimals = 0;

  for (int i = 0; input[i] != '\0'; i++)
  {
    if ((input[i] >= '0' && input[i] <= '9') || input[i] == '.' || input[i] == ',')
    {
      if (input[i] == '.' || input[i] == ',')
      {
        if (dotFound)
        {
          break;
        }
        dotFound = true;
        if (j < (int)sizeof(filteredBuffer) - 1)
        {
          filteredBuffer[j++] = '.';
        }
        continue;
      }
      if (dotFound)
      {
        decimals++;
      }
      if (j < (int)sizeof(filteredBuffer) - 1)
      {
        filteredBuffer[j++] = input[i];
      }
    }
  }
  filteredBuffer[j] = '\0'; // Null-terminate the string

  // Set the decimal places count
  *decimalPlaces = decimals;

  // Convert the cleaned buffer to a float
  *value = atof(filteredBuffer);
}

bool containsIgnoreCase(const char *text, const char *needle)
{
  if ((text == NULL) || (needle == NULL) || (*needle == '\0'))
  {
    return false;
  }

  for (; *text != '\0'; text++)
  {
    const char *h = text;
    const char *n = needle;
    while ((*h != '\0') && (*n != '\0') && (tolower((unsigned char)*h) == tolower((unsigned char)*n)))
    {
      h++;
      n++;
    }
    if (*n == '\0')
    {
      return true;
    }
  }
  return false;
}

void readWeight()
{
  if (Serial1.available())
  {
    char buff[64];
    size_t bytesRead = Serial1.readBytesUntil(0x0A, buff, sizeof(buff) - 1);
    buff[bytesRead] = '\0';

    DEBUG_PRINTLN(buff);

    if ((strchr(buff, '.') != NULL) || (strchr(buff, ',') != NULL))
    {
      weight = 0;
      dec_places = 0;
      stringToWeight(buff, &weight, &dec_places);

      if (strchr(buff, '-') != NULL) // Check if the weight is negative
      {
        weight = weight * (-1.0);
      }

      if (containsIgnoreCase(buff, "g"))
      {
        unit = " g";
        if (containsIgnoreCase(buff, "gn") || containsIgnoreCase(buff, "gr"))
        {
          unit = " gn";
        }
      }

      DEBUG_PRINT("Weight: ");
      DEBUG_PRINTLN(weight);
      lastScaleWeightReadTime = millis();
      DEBUG_PRINT("dec_places: ");
      DEBUG_PRINTLN(dec_places);

      DEBUG_PRINT("Weight Counter: ");
      DEBUG_PRINTLN(weightCounter);

      float stableTolerance = 0.5;
      for (int i = 0; (i < dec_places) && (i < 6); i++)
      {
        stableTolerance *= 0.1;
      }

      if (fabs(weight - lastWeight) <= stableTolerance)
      {
        if (running || calibrationProfilePromptPending)
        {
          if (weightCounter >= measurementCount)
          {
            newData = true;
            weightCounter = 0;
          }
          weightCounter++;
        }
      }
      else
      {
        weightCounter = 0;
        setWeightLabel(ui_LabelTricklerWeight);
      }
      lastWeight = weight;

      // DEBUG_PRINT("Scale Read: ");
      // DEBUG_PRINTLN(weight);
    }

  }
  else
  {
    bool timeout = false;
    if (strcmp(config.scale_protocol, "GG") == 0)
    {
      Serial1.write(0x1B);
      Serial1.write(0x70);
      Serial1.write(0x0D);
      Serial1.write(0x0A);

      timeout = serialWait();
    }
    else if (strcmp(config.scale_protocol, "AD") == 0)
    {
      Serial1.write("Q\r\n");
      timeout = serialWait();
    }
    else if (strcmp(config.scale_protocol, "KERN") == 0)
    {
      Serial1.write("w");
      timeout = serialWait();
    }
    else if (strcmp(config.scale_protocol, "KERN-ABT") == 0)
    {
      Serial1.write("D05\r\n");
      timeout = serialWait();
    }
    else if (strcmp(config.scale_protocol, "SBI") == 0)
    {
      Serial1.write("P\r\n");
      timeout = serialWait();
    }
    else if (strcmp(config.scale_protocol, "CUSTOM") == 0)
    {
      Serial1.write(config.scale_customCode);
      timeout = serialWait();
    }
    else
    {
      timeout = serialWait();
    }
    if (timeout)
    {
      updateDisplayLog("Timeout! Check RS232 Wiring & Settings!", true);
      delay(500);
      newData = false;
    }
  }
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
  setWeightLabel(ui_LabelTricklerWeight);

  if (newData && (lastScaleWeightReadTime > calibrationProfilePromptTime))
  {
    calibrationProfilePromptPending = false;
    newData = false;
    weightCounter = 0;
    if (confirmBox(String("Create profile from calibration?\n\nWeight: ") + String(weight, 3) + " gn", &lv_font_montserrat_14, lv_color_hex(0xFFFFFF)))
    {
      String profileName = "";
      if (createProfileFromCalibration(weight, profileName))
      {
        messageBox(String("Profile created:\n\n") + profileName + ".txt", &lv_font_montserrat_14, lv_color_hex(0x00FF00), true);
      }
      else
      {
        messageBox("Could not create profile!", &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
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

      DEBUG_PRINT("Weight: ");
      DEBUG_PRINTLN(weight);
      DEBUG_PRINT("TargetWeight: ");
      DEBUG_PRINTLN(String((config.targetWeight - tolerance), 5));
      DEBUG_PRINTLN(String((config.targetWeight + tolerance), 5));
      DEBUG_PRINT("profile_alarmThreshold: ");
      DEBUG_PRINTLN(String((config.targetWeight + alarmThreshold), 5));

      if ((weight >= (config.targetWeight - tolerance - EPSILON)) && (weight >= 0))
      {
        if (weight <= (config.targetWeight + tolerance + EPSILON))
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
          updateDisplayLog("!Over trickle!", true);
          beep("done");
          delay(250);
          beep("done");
          delay(250);
          beep("done");
          stopTrickler();
          messageBox("!Over trickle!\n!Check weight!", &lv_font_montserrat_32, lv_color_hex(0xFF0000), true);
        }

        if (!finished)
        {
          beep("done");
          updateDisplayLog("Done :)", true);
        }
        measurementCount = 0;
        finished = true;
      }

      if (!finished)
      {
        if (weight >= 0)
        {
          updateDisplayLog("Running...", true);
          setLabelTextColor(ui_LabelTricklerWeight, 0xFFFFFF);

          DEBUG_PRINTLN("Profile Running");
          String infoText = "";
          infoText.reserve(48);
          if (firstThrow)
          {
            firstThrow = false;
            if (!runBulkStepperMove(infoText))
            {
              updateDisplayLog("Bulk trickle failed!", true);
              stopTrickler();
              return;
            }
            if (infoText.length() > 0)
            {
              if (strcmp(config.profile, "calibrate") == 0)
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
            updateDisplayLog("Invalid profile!", true);
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
            updateDisplayLog("Invalid stepper number!", true);
            stopTrickler();
            return;
          }

          step(config.profile_num[profileStep], config.profile_steps[profileStep], config.profile_reverse[profileStep]);

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

          if (strcmp(config.profile, "calibrate") == 0)
          {
            startCalibrationProfilePrompt();
            return;
          }

          updateDisplayLog(infoText, true);
        }
      }

      if (weight <= 0.0000)
      {
        if (finished)
        {
          updateDisplayLog("Ready", true);
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
      updateDisplayLog("Get Weight...", true);
      readWeight();
      setWeightLabel(ui_LabelTricklerWeight);
    }
  }

  if (WIFI_AKTIVE && !running)
  {
    // if WiFi is down, try reconnecting every wifiInterval seconds
    if ((WiFi.status() != WL_CONNECTED) && (millis() - wifiPreviousMillis >= wifiInterval))
    {
      updateDisplayLog("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
      wifiPreviousMillis = millis();
    }
  }
  // esp_task_wdt_reset();
}
