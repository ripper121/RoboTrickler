#include "pindef.h"
#include <Preferences.h>
#include <Update.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "Free_Fonts.h" // Include the header file attached to this sketch
#include <TFT_eWidget.h> // Widget library
#include "SPI.h"
#include "TFT_eSPI.h"
#include "pindef.h"
#include "A4988.h"
#include <ArduinoJson.h>

#define FW_VERSION 1.18

SPIClass *SDspi = NULL;

const char *filename = "/config.txt";  // <- SD library uses 8.3 filenames
StaticJsonDocument<1536> doc;

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

struct Config {
  char wifi_ssid[64];
  char wifi_psk[64];
  bool arduino_ota;
  bool debugLog;
  char scale_protocol[16];
  int scale_baud;
  char powder[64];
  int microsteps;
  int trickler_num[32];
  float trickler_weight[32];
  int trickler_steps[32];
  int trickler_speed[32];
  int trickler_measurements[32];
  bool trickler_oscillate[32];
  bool trickler_reverse[32];
  int trickler_count;
  _mode mode;
  int log_measurements;
  _beeper beeper;
};
Config config;                         // <- global configuration object

Preferences preferences;

bool WIFI_UPDATE = false;
WebServer server(80);
bool OTA_running = false;

TFT_eSPI tft = TFT_eSPI();
#define LCD_WIDTH               480
#define LCD_HEIGHT              320
#define BUTTON_H 45
ButtonWidget btnSub = ButtonWidget(&tft);
ButtonWidget btnAdd = ButtonWidget(&tft);
ButtonWidget labelTarget = ButtonWidget(&tft);
ButtonWidget labelSpeed = ButtonWidget(&tft);
ButtonWidget labelWeight = ButtonWidget(&tft);
ButtonWidget btnStart = ButtonWidget(&tft);
ButtonWidget btnStop = ButtonWidget(&tft);
ButtonWidget btn1 = ButtonWidget(&tft);
ButtonWidget btn10 = ButtonWidget(&tft);
ButtonWidget btn100 = ButtonWidget(&tft);
ButtonWidget btn1000 = ButtonWidget(&tft);
ButtonWidget labelBanner = ButtonWidget(&tft);
ButtonWidget labelInfo = ButtonWidget(&tft);
ButtonWidget* btn[] = {&btnSub , &labelTarget, &btnAdd, &btnStart, &btnStop, &btn1, &btn10, &btn100, &btn1000, &labelWeight, &labelSpeed, &labelBanner, &labelInfo};
uint8_t buttonCount = 13;

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
int avrWeight = 0;
float newData = false;
bool running = false;
bool finished = false;
String targetWeightStr = "";
String lastTargetWeightStr = "";
int logCounter = 1;
bool stopLogCounter = false;
String path = "empty";

void beep(_beeper beepMode) {
  if (config.beeper == done && beepMode == done)
    stepper1.beep(500);
  if (config.beeper == button && beepMode == button)
    stepper1.beep(100);

  if (config.beeper == both && beepMode == done)
    stepper1.beep(500);
  if (config.beeper == both && beepMode == button)
    stepper1.beep(100);
}

bool stringContains(const String &haystack, const String &needle) {
  // Use the indexOf() method to search for the needle in the haystack
  // If the needle is found, indexOf() returns the starting position; otherwise, it returns -1
  return (haystack.indexOf(needle) != -1);
}

void setup() {
  Serial.begin(115200);

  Serial.println("RoboTrickler v" + String(FW_VERSION, 2));

  String infoText = "";
  config.mode = init_mode;

  tft_TS35_init();
  initStepper();
  initButtons();

  SDspi = new SPIClass(HSPI);
  SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  if (!SD.begin(GRBL_SPI_SS, *SDspi)) {
    Serial.println("Card Mount Failed");
    labelInfo.drawButton(false, "Card Mount Failed");
    delay(5000);
    ESP.restart();
  } else {
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
      labelInfo.drawButton(false, "No SD card attached");
      delay(5000);
      ESP.restart();
    } else {
      Serial.print("SD Card Type: ");
      if (cardType == CARD_MMC) {
        Serial.println("MMC");
      } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
      } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
      } else {
        Serial.println("UNKNOWN");
        labelInfo.drawButton(false, "CardType UNKNOWN");
      }

      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %lluMB\n", cardSize);
    }
  }

  int configState = loadConfiguration(filename, config);
  if (configState == 1) {
    infoText += "config.txt deserializeJson() failed";
  }
  if (configState == 2) {
    infoText += "powder.txt deserializeJson() failed";
  }

  if (config.mode == trickler) {
    Serial.println("config.mode == trickler");
    infoText += "Powder:" + String(config.powder) + " ";
    avrWeight = config.trickler_measurements[0];
  } else if (config.mode == logger) {
    Serial.println("config.mode == logger");
    infoText += "Logger Mode ";
    avrWeight = config.log_measurements;
  } else {
    Serial.println("config.mode == undefined");
  }

  Serial1.begin(config.scale_baud, SERIAL_8N1, IIC_SCL, IIC_SDA);

  labelInfo.drawButton(false, "Firmware Update...");
  initUpdate();

  labelInfo.drawButton(false, "Connecting Wifi...");
  initWebServer();
  if (WIFI_UPDATE) {
    infoText += "WIFI:";
    infoText += WiFi.localIP().toString();
  }

  if (config.mode == trickler) {
    preferences.begin("app-settings", false);
    targetWeight = preferences.getFloat("targetWeight", 0.0);
    if (targetWeight < 0) {
      targetWeight = 0.0;
      preferences.putFloat("targetWeight", targetWeight);
    }
    if (targetWeight > MAX_TARGET_WEIGHT) {
      targetWeight = MAX_TARGET_WEIGHT;
      preferences.putFloat("targetWeight", targetWeight);
    }
  }


  initButtons();
  labelInfo.drawButton(false, infoText);
  Serial.println("Setup done.");
}

void loop() {
  if (!OTA_running) {
    static uint32_t scanTime = millis();
    static uint32_t writeETime = millis();
    uint16_t t_x = 9999, t_y = 9999; // To store the touch coordinates

    // Scan keys every 50ms at most
    if (millis() - scanTime >= 50) {
      // Pressed will be set true if there is a valid touch on the screen
      bool pressed = tft.getTouch(&t_x, &t_y);
      t_x = LCD_WIDTH - t_x;
      t_y = LCD_HEIGHT - t_y;
      scanTime = millis();
      for (uint8_t b = 0; b < buttonCount; b++) {
        if (pressed) {
          if (btn[b]->contains(t_x, t_y)) {
            btn[b]->press(true);
            btn[b]->pressAction();
          }
        }
        else {
          btn[b]->press(false);
          btn[b]->releaseAction();
        }
      }

      if (targetWeightStr != lastTargetWeightStr)
        labelTarget.drawButton(false, targetWeightStr);
      lastTargetWeightStr = targetWeightStr;
    }

    if (millis() - writeETime >= 1000) {
      writeETime = millis();
      if (lastTargetWeight != targetWeight) {
        preferences.putFloat("targetWeight", targetWeight);
      }
      lastTargetWeight = targetWeight;

      //char temp[300];
      //sprintf(temp, "Heap: Free:%i, Min:%i, Size:%i, Alloc:%i", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
      //Serial.println(temp);
    }

    if (running) {
      if (Serial1.available()) {
        char buff[16];
        Serial1.readBytesUntil(0x0A, buff, sizeof(buff));
        if (config.debugLog) {
          labelWeight.drawButton(false, String(buff));
          debugLog(SD, String(buff) + "\n");
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
        Serial.print("separator: ");
        Serial.println(separator, DEC);

        if (separator != -1) {
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

          if (String(buff).indexOf("g") != -1 || String(buff).indexOf("G") != -1) {
            unit = "g";
            if (String(buff).indexOf("gn") != -1 || String(buff).indexOf("GN") != -1 || String(buff).indexOf("gr") != -1 || String(buff).indexOf("GR") != -1) {
              unit = "gn";
            }
          }

          if (String(buff).indexOf('-') != -1) {
            weight = weight * (-1.0);
          }

          if (lastWeight == weight) {
            weightCounter++;
            if (weightCounter > avrWeight) {
              newData = true;
              weightCounter = 0;
            }
          } else {
            weightCounter = 0;
            if (!config.debugLog) {
              labelWeight.drawButton(false, String(weight, 3) + unit);
            }
          }
          lastWeight = weight;
          Serial.print("Scale Read: ");
          Serial.println(weight, 3);
        }

      } else {
        if ((String(config.scale_protocol) == "GUG") || (String(config.scale_protocol) == "GG")) { //GUG only for backwards compatibility
          Serial1.write(0x1B);
          Serial1.write(0x70);
          Serial1.write(0x0D);
          Serial1.write(0x0A);
          delay(50);
        }
        if (String(config.scale_protocol) == "AD") {
          Serial1.write("Q\r\n");
          delay(50);
        }
        if (String(config.scale_protocol) == "KERN") {
          Serial1.write("w");
          delay(50);
        }
      }

      if (config.mode == trickler) {
        if (((targetWeight - 0.0001) <= weight) && (weight >= 0)) {
          targetWeightStr = String(targetWeight, 3);
          if (!finished) {
            beep(done);
          }
          finished = true;
          labelInfo.drawButton(false, "Done :)");
        } else {
          targetWeightStr =  String(targetWeight, 3);
        }
      }

      if (newData) {
        weightCounter = 0;
        if (!finished) {
          if ((config.mode == trickler) && (weight < targetWeight) && (weight >= 0)) {
            labelInfo.drawButton(false, "Running...");
            int stepperSpeedOld = 0;
            int profileStep = 0;
            //Serial.print("abs: ");
            //Serial.println(abs(weight - targetWeight), 3);
            //Serial.print("trickler_weight: ");
            //Serial.println(config.trickler_weight[i], 3);

            for (int i = 0; i < config.trickler_count; i++) {

              if ((weight) <= (targetWeight - config.trickler_weight[i])) {
                profileStep = i;
                break;
              }
            }

            //Serial.print("Speed: ");
            //Serial.println(config.trickler_speed[profileStep], DEC);
            if (stepperSpeedOld != config.trickler_speed[profileStep])
              setStepperSpeed(config.trickler_num[profileStep], config.trickler_speed[profileStep]); //only change if value changed
            stepperSpeedOld = config.trickler_speed[profileStep];

            //Serial.print("Stepp: ");
            //Serial.println(config.trickler_steps[profileStep], DEC);
            if (config.trickler_num[profileStep] == 1)
              stepper1.enable();
            if (config.trickler_num[profileStep] == 2)
              stepper2.enable();

            if (config.trickler_steps[profileStep] > 360 && config.trickler_oscillate[profileStep]) {
              //do full rotations
              for (int j = 0; j < int(config.trickler_steps[profileStep] / 360); j++)
                step(config.trickler_num[profileStep], config.trickler_steps[profileStep], config.trickler_oscillate[profileStep], config.trickler_reverse[profileStep]);
              //do remaining steps
              step(config.trickler_num[profileStep], (config.trickler_steps[profileStep] % 360), config.trickler_oscillate[profileStep], config.trickler_reverse[profileStep]);
            }
            else
              step(config.trickler_num[profileStep], config.trickler_steps[profileStep], config.trickler_oscillate[profileStep], config.trickler_reverse[profileStep]);
            stepper1.disable();
            stepper2.disable();

            //Serial.print("Measurements: ");
            //Serial.println(config.trickler_measurements[profileStep], DEC);
            avrWeight = config.trickler_measurements[profileStep];

            String infoText = "";
            infoText += "W" + String(config.trickler_weight[profileStep], 3) + " ";
            infoText += "ST" + String(config.trickler_steps[profileStep]) + " ";
            infoText += "SP" + String(config.trickler_speed[profileStep]) + " ";
            infoText += "M" + String(config.trickler_measurements[profileStep]);
            labelInfo.drawButton(false, infoText);
          }
          if (config.mode == logger && (weight > 0)) {
            Serial.println("Log to File");
            if (path == "empty") {
              path = ceateCSVFile(SD, "/log", "log");
            }
            String infoText = "";
            writeCSVFile(SD, path.c_str(), weight, logCounter);
            avrWeight = config.log_measurements;
            infoText += "Count: " + String(logCounter) + " Saved :)";
            labelInfo.drawButton(false, infoText);
            finished = true;
            beep(done);
          }
        } else {
          if (config.mode == trickler) {
            stepper1.disable();
            stepper2.disable();
          }
        }

        if ((weight <= 0) && finished) {
          if (config.mode == logger) {
            logCounter++;
            labelInfo.drawButton(false, "Place next peace for measurment.");
            beep(done);
          }
          finished = false;
        }

        newData = false;
        while (Serial1.available()) {
          Serial1.read();
        }
      }
    } else {
      stepper1.disable();
      stepper2.disable();
      path = "empty";
      logCounter = 1;
    }
  }


  if (WIFI_UPDATE && !running) {
    if (WiFi.status() == WL_CONNECTED) {
      server.handleClient();
      if (config.arduino_ota) {
        ArduinoOTA.handle();
      }
    }
  }
}
