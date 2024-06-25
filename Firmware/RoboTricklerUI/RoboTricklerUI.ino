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
#include <QuickPID.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <soc/rtc_wdt.h>
#define FW_VERSION 2.04

// 3 seconds WDT
#define WDT_TIMEOUT 10

#include <lvgl.h>
#include "ui.h"
#include <TFT_eSPI.h>

#define DEBUG 0
#ifdef DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#define DISP_TASK_STACK 4096 * 2
#define DISP_TASK_PRO 1
#define DISP_TASK_CORE 0
TaskHandle_t lv_disp_tcb = NULL;
/*Change to your screen resolution*/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10];
TFT_eSPI tft = TFT_eSPI(LV_HOR_RES_MAX, LV_VER_RES_MAX); /* TFT instance */

SPIClass *SDspi = NULL;

String fullLogOutput = "";

struct Config
{
  char wifi_ssid[64];
  char wifi_psk[64];
  char IPStatic[15];
  char IPGateway[15];
  char IPSubnet[15];
  char IPDns[15];

  char mode[9];
  char beeper[6];
  bool debugLog;
  bool fwCheck;
  float weight;
  char scale_protocol[32];
  int scale_baud;
  char scale_customCode[32];
  char profile[32];
  int log_measurements;
  int microsteps;

  float pidThreshold;
  long pidStepMin;
  long pidStepMax;
  int pidSpeed;
  int pidConMeasurements;
  int pidAggMeasurements;
  float pidTolerance;
  float pidAlarmThreshold;
  bool pidOscillate;
  bool pidReverse;
  bool pidAcceleration;
  byte pidConsNum;
  float pidConsKp;
  float pidConsKi;
  float pidConsKd;
  byte pidAggNum;
  float pidAggKp;
  float pidAggKi;
  float pidAggKd;

  byte profile_num[16];
  float profile_weight[16];
  double profile_stepsPerUnit;
  float profile_tolerance;
  float profile_alarmThreshold;
  int profile_measurements[16];
  long profile_steps[16];
  int profile_speed[16];
  bool profile_oscillate[16];
  bool profile_reverse[16];
  bool profile_acceleration[16];
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
int dec_places = 3;
String unit = "";
float tolerance;
float alarmThreshold;
float targetWeight = 0.0;
float lastWeight = 0;
float addWeight = 0.1;
int weightCounter = 0;
int measurementCount = 0;
float newData = false;
bool running = false;
bool finished = false;
bool firstThrow = true;
int logCounter = 1;
String path = "empty";

// Define the aggressive and conservative and POn Tuning Parameters
float Input, Output;
// Specify the links
QuickPID roboPID(&Input, &Output, &targetWeight);
bool PID_AKTIVE = false;

String infoMessagBuff[14];
String profileListBuff[32];
byte profileListCount;

void updateDisplayLog(String logOutput, bool noLog = false)
{
  lv_label_set_text(ui_LabelInfo, logOutput.c_str());
  lv_label_set_text(ui_LabelLoggerInfo, logOutput.c_str());
  logOutput += "\n";

  if (!noLog)
  {
    insertLine(logOutput);
    String temp = "";
    for (int i = 0; i < (sizeof(infoMessagBuff) / sizeof(infoMessagBuff[0])); i++)
    {
      temp += infoMessagBuff[i];
    }
    lv_label_set_text(ui_LabelLog, temp.c_str());
  }

  DEBUG_PRINT(logOutput);
}

void readWeight()
{
  if (Serial1.available())
  {
    char buff[64];
    Serial1.readBytesUntil(0x0A, buff, sizeof(buff));
    
    DEBUG_PRINTLN(String(buff));

    if (config.debugLog)
    {
      lv_label_set_text(ui_LabelTricklerWeight, String(buff).c_str());
      logToFile(SD, String(buff) + "\n");
    }

    weight = 0.0;

    int readWeigt[8];

    int separator = String(buff).indexOf('.');
    // DEBUG_PRINT("separator: ");
    // DEBUG_PRINTLN(separator);

    int N10P3 = 0;
    int N10P2 = 0;
    int N10P1 = 0;
    int N10N1 = 0;
    int N10N2 = 0;
    int N10N3 = 0;

    if (separator != -1)
    {
      N10P3 = (buff[separator - 3] > 0x30) ? (buff[separator - 3] - 0x30) : 0;
      N10P2 = (buff[separator - 2] > 0x30) ? (buff[separator - 2] - 0x30) : 0;
      N10P1 = (buff[separator - 1] > 0x30) ? (buff[separator - 1] - 0x30) : 0;
      N10N1 = (buff[separator + 1] > 0x30) ? (buff[separator + 1] - 0x30) : 0;
      N10N2 = (buff[separator + 2] > 0x30) ? (buff[separator + 2] - 0x30) : 0;
      N10N3 = (buff[separator + 3] > 0x30) ? (buff[separator + 3] - 0x30) : 0;

      weight = N10P3 * 100.0;
      weight += N10P2 * 10.0;
      weight += N10P1 * 1.0;
      weight += (N10N1 == 0) ? (0.0) : (N10N1 / 10.0);
      weight += (N10N2 == 0) ? (0.0) : (N10N2 / 100.0);
      weight += (N10N3 == 0) ? (0.0) : (N10N3 / 1000.0);

      if (String(buff).indexOf("g") != -1 || String(buff).indexOf("G") != -1)
      {
        unit = "g";
        if (String(buff).indexOf("gn") != -1 || String(buff).indexOf("GN") != -1 || String(buff).indexOf("gr") != -1 || String(buff).indexOf("GR") != -1)
        {
          unit = "gn";
        }
      }

      if (String(buff).indexOf('-') != -1)
      {
        weight = weight * (-1.0);
      }

      // DEBUG_PRINT("Weight Counter: ");
      // DEBUG_PRINTLN(weightCounter);

      if (lastWeight == weight)
      {
        if (weightCounter >= measurementCount)
        {
          newData = true;
          weightCounter = 0;
        }
        weightCounter++;
      }
      else
      {
        weightCounter = 0;
        if (!config.debugLog)
        {
          lv_label_set_text(ui_LabelTricklerWeight, String(String(weight, dec_places) + unit).c_str());
          lv_label_set_text(ui_LabelLoggerWeight, String(String(weight, dec_places) + unit).c_str());
        }
      }
      lastWeight = weight;

      // DEBUG_PRINT("Scale Read: ");
      // DEBUG_PRINTLN(weight);
    }

    serialFlush();
  }
  else
  {
    bool timeout = false;
    if ((String(config.scale_protocol) == "GUG") || (String(config.scale_protocol) == "GG"))
    { // GUG only for backwards compatibility
      Serial1.write(0x1B);
      Serial1.write(0x70);
      Serial1.write(0x0D);
      Serial1.write(0x0A);
      // The G&G PLC100BC dosent like to be asked to often via RS232, the Values get unstabel, so we wait little more
      // delay(200);
      timeout = serialWait();
    }
    else if (String(config.scale_protocol) == "AD")
    {
      Serial1.write("Q\r\n");
      timeout = serialWait();
    }
    else if (String(config.scale_protocol) == "KERN")
    {
      Serial1.write("w");
      timeout = serialWait();
    }
    else if (String(config.scale_protocol) == "KERN-ABT")
    {
      Serial1.write("D05\r\n");
      timeout = serialWait();
    }
    else if (String(config.scale_protocol) == "SBI")
    {
      Serial1.write("P\r\n");
      timeout = serialWait();
    }
    else if (String(config.scale_protocol) == "CUSTOM")
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
      updateDisplayLog("Scale Data Timeout!");
      delay(500);
      updateDisplayLog("Check RS232 Wiring & Settings!");
      delay(500);
      newData = false;
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
    config.weight = targetWeight;

    char temp[300];
    sprintf(temp, "Heap: Free:%i, Min:%i, Size:%i, Alloc:%i", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
    DEBUG_PRINTLN(temp);

    printf("lv_disp_tcb stackHWM: %d / %d\n", (DISP_TASK_STACK - uxTaskGetStackHighWaterMark(lv_disp_tcb)), DISP_TASK_STACK);
    printf("loop stackHWM: %d / %d\n", (getArduinoLoopTaskStackSize() - uxTaskGetStackHighWaterMark(NULL)), getArduinoLoopTaskStackSize());
  }

  if (running)
  {
    readWeight();

    if (newData)
    {
      newData = false;
      weightCounter = 0;

      if (String(config.mode).indexOf("trickler") != -1)
      {
        DEBUG_PRINT("Weight: ");
        DEBUG_PRINTLN(weight);
        DEBUG_PRINT("TargetWeight: ");
        DEBUG_PRINTLN(String((targetWeight - tolerance), 3));
        DEBUG_PRINT("alarmThreshold: ");
        DEBUG_PRINTLN(String((targetWeight + alarmThreshold), 3));

        // adding 0.00001 to weight, otherwise it would always go over the targetWeight
        if (((weight + 0.0001) >= (targetWeight - tolerance)) && (weight >= 0))
        {
          if (!finished)
          {
            beep("done");
            updateDisplayLog("Done :)", true);
            serialFlush();
            firstThrow = true;
          }

          if ((weight >= (targetWeight + alarmThreshold)) && (alarmThreshold > 0))
          {
            // Send Alarm
            messageBox("!Over trickle!\n!Check weight!", &lv_font_montserrat_32, lv_color_hex(0xFF0000));
            beep("done");
            delay(250);
            beep("done");
            delay(250);
            beep("done");

            serialFlush();
            stopTrickler();
          }

          measurementCount = 0;
          finished = true;
        }

        if ((weight + 0.0001) < (targetWeight - tolerance))
        {
          finished = false;
        }
      }
      if (!finished)
      {
        if ((String(config.mode).indexOf("trickler") != -1) && (weight < targetWeight) && (weight >= 0))
        {
          updateDisplayLog("Running...", true);

          if (PID_AKTIVE)
          {
            DEBUG_PRINTLN("PID Running");
            String infoText = "";
            byte stepperNum = 1;
            int stepperSpeedOld = 0;
            Input = weight;

            alarmThreshold = config.pidAlarmThreshold;
            tolerance = config.pidTolerance;

            float gap = abs(targetWeight - Input); // distance away from setpoint

            if (gap <= config.pidThreshold)
            { // we're close to setpoint, use conservative tuning parameters
              roboPID.SetTunings(config.pidConsKp, config.pidConsKi, config.pidConsKd);
              measurementCount = config.pidConMeasurements;
              stepperNum = config.pidConsNum;
              infoText += "GOV:CON ";
            }
            else
            {
              // we're far from setpoint, use aggressive tuning parameters
              roboPID.SetTunings(config.pidAggKp, config.pidAggKi, config.pidAggKd);
              measurementCount = config.pidAggMeasurements;
              stepperNum = config.pidAggNum;
              infoText += "GOV:AGG ";
            }
            roboPID.Compute();

            long steps = (long)Output;

            infoText += "GAP:" + String(gap, 3) + " ";
            infoText += "STP:" + String(steps) + " ";
            infoText += "MES:" + String(measurementCount);
            updateDisplayLog(infoText, true);

            if (stepperSpeedOld != config.pidSpeed)
              setStepperSpeed(stepperNum, config.pidSpeed); // only change if value changed
            stepperSpeedOld = config.pidSpeed;
            step(stepperNum, steps, config.pidOscillate, config.pidReverse, config.pidAcceleration);
          }
          else
          {
            DEBUG_PRINTLN("Profile Running");
            String infoText = "";
            DEBUG_PRINT("firstThrow: ");
            DEBUG_PRINTLN(firstThrow);
            if (firstThrow && config.profile_stepsPerUnit > 0)
            {
              double steps = (targetWeight - weight) * config.profile_stepsPerUnit;
              DEBUG_PRINT("FirstThrow steps: ");
              DEBUG_PRINTLN(steps);
              setStepperSpeed(1, config.profile_speed[0]);
              step(1, steps, config.profile_oscillate[0], config.profile_reverse[0], config.profile_acceleration[0]);
              infoText += "FirstThrow steps:" + String(steps);
              firstThrow = false;
            }
            else
            {
              int stepperSpeedOld = 0;
              int profileStep = 0;
              alarmThreshold = config.profile_alarmThreshold;
              tolerance = config.profile_tolerance;
              // Serial.print("abs: ");
              // Serial.println(abs(weight - targetWeight), 3);
              // Serial.print("profile_weight: ");
              // Serial.println(config.profile_weight[i], 3);

              for (int i = 0; i < config.profile_count; i++)
              {

                if ((weight) <= (targetWeight - config.profile_weight[i]))
                {
                  profileStep = i;
                  break;
                }
              }

              // Serial.print("Speed: ");
              // Serial.println(config.profile_speed[profileStep], DEC);
              if (stepperSpeedOld != config.profile_speed[profileStep])
                setStepperSpeed(config.profile_num[profileStep], config.profile_speed[profileStep]); // only change if value changed
              stepperSpeedOld = config.profile_speed[profileStep];

              // Serial.print("Stepp: ");
              // Serial.println(config.profile_steps[profileStep], DEC);

              if (config.profile_steps[profileStep] > 360 && config.profile_oscillate[profileStep])
              {
                // do full rotations
                for (int j = 0; j < int(config.profile_steps[profileStep] / 360); j++)
                  step(config.profile_num[profileStep], 360, config.profile_oscillate[profileStep], config.profile_reverse[profileStep], config.profile_acceleration[profileStep]);
                // do remaining steps
                step(config.profile_num[profileStep], (config.profile_steps[profileStep] % 360), config.profile_oscillate[profileStep], config.profile_reverse[profileStep], config.profile_acceleration[profileStep]);
              }
              else
                step(config.profile_num[profileStep], config.profile_steps[profileStep], config.profile_oscillate[profileStep], config.profile_reverse[profileStep], config.profile_acceleration[profileStep]);

              // Serial.print("Measurements: ");
              // Serial.println(config.profile_measurements[profileStep], DEC);
              measurementCount = config.profile_measurements[profileStep];

              infoText += "W" + String(config.profile_weight[profileStep], 3) + " ";
              infoText += "ST" + String(config.profile_steps[profileStep]) + " ";
              infoText += "SP" + String(config.profile_speed[profileStep]) + " ";
              infoText += "M" + String(config.profile_measurements[profileStep]);
            }
            updateDisplayLog(infoText, true);
          }
          serialFlush();
        }
        if (String(config.mode).indexOf("logger") != -1 && (weight > 0))
        {
          DEBUG_PRINTLN("Log to File");
          if (path == "empty")
          {
            path = ceateCSVFile(SD, "/log", "log");
          }
          String infoText = "";
          writeCSVFile(SD, path.c_str(), weight, logCounter);
          measurementCount = config.log_measurements;
          infoText += "Count: " + String(logCounter) + " Saved :)";
          updateDisplayLog(infoText, true);
          finished = true;
          beep("done");
        }
      }
      else
      {
        if (String(config.mode).indexOf("trickler") != -1)
        {
          stepper1.disable();
          stepper2.disable();
        }
      }

      if ((weight <= 0) && finished)
      {
        if (String(config.mode).indexOf("logger") != -1)
        {
          logCounter++;
          updateDisplayLog("Place next peace...", true);
          beep("done");
        }
        else
        {
          updateDisplayLog("Ready", true);
        }
        finished = false;
      }
    }
  }
  else
  {
    stepper1.disable();
    stepper2.disable();
    path = "empty";
    logCounter = 1;
    if (millis() - readWeightTime >= 1000)
    {
      readWeightTime = millis();
      updateDisplayLog("Get Weight...", true);
      readWeight();
      lv_label_set_text(ui_LabelTricklerWeight, String(String(weight, dec_places) + unit).c_str());
      lv_label_set_text(ui_LabelLoggerWeight, String(String(weight, dec_places) + unit).c_str());
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
