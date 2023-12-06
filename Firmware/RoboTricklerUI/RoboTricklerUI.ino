#include "pindef.h"
#include <Preferences.h>
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

#define FW_VERSION 2.00

SPIClass *SDspi = NULL;

String fullLogOutput = "";

enum _mode
{
  init_mode,
  trickler,
  logger
};

enum _beeper
{
  off,
  done,
  button,
  both
};

struct Config
{
  char wifi_ssid[64];
  char wifi_psk[64];
  char IPStatic[15];
  char IPGateway[15];
  char IPSubnet[15];
  char IPDns[15];
  bool debugLog;
  char scale_protocol[16];
  int scale_baud;
  char profile[32];
  float tolerance;
  float alarmThreshold;
  int microsteps;
  float pidThreshold;
  long pidStepMin;
  long pidStepMax;
  int pidSpeed;
  int pidConMeasurements;
  int pidAggMeasurements;
  bool pidOscillate;
  bool pidReverse;
  byte pidConsNum;
  float pidConsKp;
  float pidConsKi;
  float pidConsKd;
  byte pidAggNum;
  float pidAggKp;
  float pidAggKi;
  float pidAggKd;
  byte profile_num[32];
  float profile_weight[32];
  long profile_steps[32];
  int profile_speed[32];
  int profile_measurements[32];
  bool profile_oscillate[32];
  bool profile_reverse[32];
  int profile_count;
  _mode mode;
  int log_measurements;
  _beeper beeper;
};
Config config; // <- global configuration object

Preferences preferences;

bool WIFI_AKTIVE = false;
WebServer server(80);
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = 10000;

#define MOTOR_STEPS 200
#define RPM 200
A4988 stepper1(MOTOR_STEPS, I2S_X_DIRECTION_PIN, I2S_X_STEP_PIN, I2S_X_DISABLE_PIN);
A4988 stepper2(MOTOR_STEPS, I2S_Y_DIRECTION_PIN, I2S_Y_STEP_PIN, I2S_Y_DISABLE_PIN);

#define MAX_TARGET_WEIGHT 100
float weight = 0.0;
float targetWeight = 0.0;
float lastTargetWeight = 0.0;
float lastWeight = 0;
float addWeight = 0.1;
int weightCounter = 0;
int measurementCount = 0;
float newData = false;
bool running = false;
bool finished = false;
String targetWeightStr = "";
String lastTargetWeightStr = "";
int logCounter = 1;
bool stopLogCounter = false;
String path = "empty";

// Define the aggressive and conservative and POn Tuning Parameters
float aggKp = 4, aggKi = 0.2, aggKd = 1;
float consKp = 1, consKi = 0.05, consKd = 0.25;
float Setpoint, Input, Output;
// Specify the links
QuickPID roboPID(&Input, &Output, &targetWeight);
bool PID_AKTIVE = false;

String infoMessagBuff[14];
String profileListBuff[32];
byte profileListCount;
byte profileListCounter;

void beep(_beeper beepMode)
{
  if (config.beeper == done && beepMode == done)
    stepper1.beep(500);
  if (config.beeper == button && beepMode == button)
    stepper1.beep(100);

  if (config.beeper == both && beepMode == done)
    stepper1.beep(500);
  if (config.beeper == both && beepMode == button)
    stepper1.beep(100);
}

bool serialWait()
{
  bool timeout = true;
  for (int i = 0; i < 5000; i++)
  {
    if (Serial1.available())
    {
      timeout = false;
      break;
    }
    else
    {
      delay(1);
    }
  }
  return timeout;
}

IRAM_ATTR void lvgl_disp_task(void *parg)
{
  while (1)
  {
    lv_timer_handler();
    // esp_task_wdt_reset();
  }
}

IRAM_ATTR void disp_task_init(void)
{
  xTaskCreatePinnedToCore(lvgl_disp_task,  // task
                          "lvglTask",      // name for task
                          DISP_TASK_STACK, // size of task stack
                          NULL,            // parameters
                          DISP_TASK_PRO,   // priority
                          // nullptr,
                          &lv_disp_tcb,
                          DISP_TASK_CORE // must run the task on same core
                                         // core
  );
}

void insertLine(String newLine)
{
  // Shift all lines up by one position
  for (int i = 0; i < (sizeof(infoMessagBuff) / sizeof(infoMessagBuff[0])) - 1; i++)
  {
    infoMessagBuff[i] = infoMessagBuff[i + 1];
  }
  // Add new line at the bottom
  infoMessagBuff[(sizeof(infoMessagBuff) / sizeof(infoMessagBuff[0])) - 1] = newLine;
}

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

void setup()
{
  Serial.begin(115200); /* prepare for possible serial debug */

  // esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  // esp_task_wdt_add(NULL);               // add current thread to WDT watch
  // esp_task_wdt_add(lv_disp_tcb);

  rtc_wdt_protect_off();
  rtc_wdt_disable();

  displayInit();
  ui_init();
  disp_task_init();

  DEBUG_PRINTLN("Display Init");

  updateDisplayLog(String("Robo-Trickler v" + String(FW_VERSION, 2) + " // ripper121.com").c_str());

  config.mode = init_mode;

  initStepper();

  SDspi = new SPIClass(HSPI);
  SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  if (!SD.begin(GRBL_SPI_SS, *SDspi))
  {
    updateDisplayLog("Card Mount Failed");
    delay(5000);
    ESP.restart();
  }
  else
  {
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
      updateDisplayLog("No SD card attached");
      delay(5000);
      ESP.restart();
    }
    else
    {
      DEBUG_PRINT("SD Card Type: ");
      if (cardType == CARD_MMC)
      {
        DEBUG_PRINTLN("MMC");
      }
      else if (cardType == CARD_SD)
      {
        DEBUG_PRINTLN("SDSC");
      }
      else if (cardType == CARD_SDHC)
      {
        DEBUG_PRINTLN("SDHC");
      }
      else
      {
        DEBUG_PRINTLN("UNKNOWN");
        updateDisplayLog("CardType UNKNOWN");
      }

      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      DEBUG_PRINT("SD Card Size: ");
      DEBUG_PRINT(cardSize);
      DEBUG_PRINTLN("MB");
    }
  }

  int configState = loadConfiguration("/config.txt", config);
  if (configState == 1)
  {
    updateDisplayLog("config.txt deserializeJson() failed");
  }
  else if (configState == 2)
  {
    updateDisplayLog("profile.txt deserializeJson() failed");
  }
  else
  {
    getProfileList();
  }

  if (config.mode == trickler)
  {
    DEBUG_PRINTLN("config.mode == trickler");
    updateDisplayLog("Profile:" + String(config.profile));
    measurementCount = config.profile_measurements[0];
  }
  else if (config.mode == logger)
  {
    DEBUG_PRINTLN("config.mode == logger");
    updateDisplayLog("Logger Mode");
    measurementCount = config.log_measurements;
  }
  else
  {
    DEBUG_PRINTLN("config.mode == undefined");
    updateDisplayLog("Mode undefined!");
  }

  Serial1.begin(config.scale_baud, SERIAL_8N1, IIC_SCL, IIC_SDA);

  initUpdate();

  initWebServer();

  if (WIFI_AKTIVE)
  {
    updateDisplayLog("WIFI:" + WiFi.localIP().toString());
  }

  if (config.mode == trickler)
  {
    preferences.begin("app-settings", false);
    targetWeight = preferences.getFloat("targetWeight", 0.0);
    if (targetWeight < 0)
    {
      targetWeight = 0.0;
      preferences.putFloat("targetWeight", targetWeight);
    }
    if (targetWeight > MAX_TARGET_WEIGHT)
    {
      targetWeight = MAX_TARGET_WEIGHT;
      preferences.putFloat("targetWeight", targetWeight);
    }
  }

  // initialize the variables we're linked to
  Input = 0;
  roboPID.SetOutputLimits(config.pidStepMin, config.pidStepMax);
  // turn the PID on
  roboPID.SetMode(roboPID.Control::automatic);

  lv_label_set_text(ui_LabelInfo, String("Robo-Trickler v" + String(FW_VERSION, 2) + " // ripper121.com").c_str());
  lv_label_set_text(ui_LabelTarget, String(targetWeight, 3).c_str());

  DEBUG_PRINTLN("Setup done.");
}

void loop()
{
  static uint32_t scanTime = millis();
  static uint32_t writeETime = millis();

  if (millis() - scanTime >= 50)
  {
    scanTime = millis();
    if (targetWeightStr != lastTargetWeightStr)
    {
      // labelTarget.drawButton(false, targetWeightStr);
    }
    lastTargetWeightStr = targetWeightStr;
  }

  if (millis() - writeETime >= 1000)
  {
    writeETime = millis();
    if (lastTargetWeight != targetWeight)
    {
      preferences.putFloat("targetWeight", targetWeight);
    }
    lastTargetWeight = targetWeight;

    char temp[300];
    sprintf(temp, "Heap: Free:%i, Min:%i, Size:%i, Alloc:%i", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
    DEBUG_PRINTLN(temp);

    printf("lv_disp_tcb stackHWM: %d / %d\n", (DISP_TASK_STACK - uxTaskGetStackHighWaterMark(lv_disp_tcb)), DISP_TASK_STACK);
    printf("loop stackHWM: %d / %d\n", (getArduinoLoopTaskStackSize() - uxTaskGetStackHighWaterMark(NULL)), getArduinoLoopTaskStackSize());
  }

  if (running)
  {
    if (Serial1.available())
    {
      char buff[64];
      Serial1.readBytesUntil(0x0A, buff, sizeof(buff));

      DEBUG_PRINTLN(buff);

      if (config.debugLog)
      {
        lv_label_set_text(ui_LabelTricklerWeight, String(buff).c_str());
        logToFile(SD, String(buff) + "\n");
      }

      bool negative = false;
      String unit = "";
      weight = 0.0;

      int N10P3 = 0;
      int N10P2 = 0;
      int N10P1 = 0;
      int N10N1 = 0;
      int N10N2 = 0;
      int N10N3 = 0;

      int separator = String(buff).indexOf('.');
      DEBUG_PRINT("separator: ");
      DEBUG_PRINTLN(separator);

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

        DEBUG_PRINT("Weight Counter: ");
        DEBUG_PRINTLN(weightCounter);

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
            lv_label_set_text(ui_LabelTricklerWeight, String(String(weight, 3) + unit).c_str());
            lv_label_set_text(ui_LabelLoggerWeight, String(String(weight, 3) + unit).c_str());
          }
        }
        lastWeight = weight;

        DEBUG_PRINT("Scale Read: ");
        DEBUG_PRINTLN(weight);
      }

      // flush TX
      Serial1.flush();
      // flush RX
      while (Serial1.available())
      {
        Serial1.read();
      }
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
      else if (String(config.scale_protocol) == "SBI")
      {
        Serial1.write("P\r\n");
        timeout = serialWait();
      }
      else
      {
        timeout = serialWait();
      }
      if (timeout)
      {
        updateDisplayLog("Scale Data Timeout!");
        delay(1000);
        updateDisplayLog("Check RS232 Wiring & Settings!");
        delay(1000);
        newData = false;
      }
    }

    if (newData)
    {
      newData = false;
      weightCounter = 0;

      if (config.mode == trickler)
      {
        if (((targetWeight - 0.0001) <= weight) && (weight >= 0))
        {
          if (!finished)
          {
            beep(done);
          }
          finished = true;
          updateDisplayLog("Done :)", true);

          if (weight > (targetWeight + (targetWeight * config.alarmThreshold)))
          {
            // Send Alarm
            messageBox("Check Weight!!!");
          }

          measurementCount = 0;
          delay(250);
        }

        if (weight < targetWeight)
        {
          finished = false;
        }

        targetWeightStr = String(targetWeight, 3);
      }
      if (!finished)
      {
        if ((config.mode == trickler) && (weight < targetWeight) && (weight >= 0))
        {
          updateDisplayLog("Running...", true);

          if (PID_AKTIVE)
          {
            DEBUG_PRINTLN("PID Running");
            String infoText = "";
            byte stepperNum = 1;
            int stepperSpeedOld = 0;
            Input = weight;

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
            step(stepperNum, steps, config.pidOscillate, config.pidReverse);
          }
          else
          {
            DEBUG_PRINTLN("Profile Running");
            int stepperSpeedOld = 0;
            int profileStep = 0;
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
                step(config.profile_num[profileStep], 360, config.profile_oscillate[profileStep], config.profile_reverse[profileStep]);
              // do remaining steps
              step(config.profile_num[profileStep], (config.profile_steps[profileStep] % 360), config.profile_oscillate[profileStep], config.profile_reverse[profileStep]);
            }
            else
              step(config.profile_num[profileStep], config.profile_steps[profileStep], config.profile_oscillate[profileStep], config.profile_reverse[profileStep]);

            // Serial.print("Measurements: ");
            // Serial.println(config.profile_measurements[profileStep], DEC);
            measurementCount = config.profile_measurements[profileStep];

            String infoText = "";
            infoText += "W" + String(config.profile_weight[profileStep], 3) + " ";
            infoText += "ST" + String(config.profile_steps[profileStep]) + " ";
            infoText += "SP" + String(config.profile_speed[profileStep]) + " ";
            infoText += "M" + String(config.profile_measurements[profileStep]);
            updateDisplayLog(infoText, true);
          }
        }
        if (config.mode == logger && (weight > 0))
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
          beep(done);
        }
      }
      else
      {
        if (config.mode == trickler)
        {
          stepper1.disable();
          stepper2.disable();
        }
      }

      if ((weight <= 0) && finished)
      {
        if (config.mode == logger)
        {
          logCounter++;
          updateDisplayLog("Place next peace for measurment.", true);
          beep(done);
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
  }

  if (WIFI_AKTIVE && !running)
  {

    if (WiFi.status() == WL_CONNECTED)
    {
      server.handleClient();
    }

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
