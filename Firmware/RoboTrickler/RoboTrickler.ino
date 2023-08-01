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
  char scale_protocol[16];
  int scale_baud;
  char powder[64];
  float trickler_weight[32];
  int trickler_steps[32];
  int trickler_speed[32];
  int trickler_measurements[32];
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
ButtonWidget labelBanner = ButtonWidget(&tft);
ButtonWidget labelInfo = ButtonWidget(&tft);
ButtonWidget* btn[] = {&btnSub , &labelTarget, &btnAdd, &btnStart, &btnStop, &btn1, &btn10, &btn100, &labelWeight, &labelSpeed, &labelBanner, &labelInfo};
uint8_t buttonCount = sizeof(btn) / sizeof(btn[0]);

#define MOTOR_STEPS 200
#define RPM 200
#define MICROSTEPS 1
A4988 stepperX(MOTOR_STEPS, I2S_X_DIRECTION_PIN, I2S_X_STEP_PIN, I2S_X_DISABLE_PIN);

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
String path = "";

void beep(_beeper beepMode) {
  if (config.beeper == done && beepMode == done)
    stepperX.beep(500);
  if (config.beeper == button && beepMode == button)
    stepperX.beep(100);

  if (config.beeper == both && beepMode == done)
    stepperX.beep(500);
  if (config.beeper == both && beepMode == button)
    stepperX.beep(100);
}

void setup() {
  Serial.begin(9600);

  Serial.println("RoboTrickler v1.1");

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
  if (WIFI_UPDATE)
    infoText += "WIFI";

  if (config.mode == trickler) {
    preferences.begin("app-settings", false);
    targetWeight = preferences.getFloat("targetWeight", 0.0);
    if (targetWeight < 0 || targetWeight > 100) {
      targetWeight = 0.0;
      preferences.putFloat("targetWeight", targetWeight);
    }
  }


  initButtons();
  labelInfo.drawButton(false, infoText);
  Serial.println("Setp done.");
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
    }
    if (running) {
      if (Serial1.available()) {
        char buff[16];
        Serial1.readBytesUntil(0x0A, buff, sizeof(buff));

        int grammH = 0;
        int grammT = 0;
        int grammO = 0;
        int milliH = 0;
        int milliT = 0;
        int milliO = 0;

        if (String(config.scale_protocol) == "GUG") {
          grammH = (buff[2] > 0x30) ? (buff[2] - 0x30) : 0;
          grammT = (buff[3] > 0x30) ? (buff[3] - 0x30) : 0;
          grammO = (buff[4] > 0x30) ? (buff[4] - 0x30) : 0;
          milliH = (buff[6] > 0x30) ? (buff[6] - 0x30) : 0;
          milliT = (buff[7] > 0x30) ? (buff[7] - 0x30) : 0;
          milliO = (buff[8] > 0x30) ? (buff[8] - 0x30) : 0;
        }
        if (String(config.scale_protocol) == "SBI") {
          grammH = (buff[3] > 0x30) ? (buff[3] - 0x30) : 0;
          grammT = (buff[4] > 0x30) ? (buff[4] - 0x30) : 0;
          grammO = (buff[5] > 0x30) ? (buff[5] - 0x30) : 0;
          milliH = (buff[7] > 0x30) ? (buff[7] - 0x30) : 0;
          milliT = (buff[8] > 0x30) ? (buff[8] - 0x30) : 0;
          milliO = (buff[9] > 0x30) ? (buff[9] - 0x30) : 0;
        }

        weight = grammH * 100.0;
        weight += grammT * 10.0;
        weight += grammO * 1.0;
        weight += (milliH == 0) ? (0.0) : (milliH / 10.0);
        weight += (milliT == 0) ? (0.0) : (milliT / 100.0);
        weight += (milliO == 0) ? (0.0) : (milliO / 1000.0);



        if (abs(lastWeight - weight) < 0.0001) {
          weightCounter++;
          if (weightCounter > avrWeight) {
            newData = true;
            weightCounter = 0;
          }
        } else {
          weightCounter = 0;
          if (config.mode == trickler)
            labelWeight.drawButton(false, "Diff: " + String((targetWeight - weight), 3) + " g");
          else if (config.mode == logger)
            labelWeight.drawButton(false, String(weight, 3) + " g");
        }
        lastWeight = weight;
        Serial.print("Scale Read: ");
        Serial.println(weight, 3);
      } else {
        if (String(config.scale_protocol) == "GUG") {
          Serial1.write(0x1B);
          Serial1.write(0x70);
          Serial1.write(0x0D);
          Serial1.write(0x0A);
        }
      }

      if (config.mode == trickler) {
        if ((abs(weight - targetWeight) < 0.0001)) {
          targetWeightStr = " > " + String(targetWeight, 3) + " g < ";
          if (!finished) {
            beep(done);
          }
          finished = true;
          labelInfo.drawButton(false, "Done :)");
        } else {
          targetWeightStr =  String(targetWeight, 3) + " g";
        }
      }

      if (newData) {
        if (!finished) {
          if ((config.mode == trickler) && (weight < targetWeight)) {
            labelInfo.drawButton(false, "Running...");
            stepperX.enable();
            int stepperSpeedOld = 0;
            for (int i = 0; i < config.trickler_count; i++) {
              //Serial.print("abs: ");
              //Serial.println(abs(weight - targetWeight), 3);
              //Serial.print("trickler_weight: ");
              //Serial.println(config.trickler_weight[i], 3);

              if (weight < (targetWeight - config.trickler_weight[i])) {

                //Serial.print("Speed: ");
                //Serial.println(config.trickler_speed[i], DEC);
                if (stepperSpeedOld != config.trickler_speed[i])
                  setStepperSpeed(config.trickler_speed[i]); //only change if value changed
                stepperSpeedOld = config.trickler_speed[i];

                //Serial.print("Stepp: ");
                //Serial.println(config.trickler_steps[i], DEC);
                if (config.trickler_steps[i] > 360) {
                  //do full rotations
                  for (int j = 0; j < int(config.trickler_steps[i] / 360); j++)
                    step(360);
                  //do remaining steps
                  step((config.trickler_steps[i] % 360));
                }
                else
                  step(config.trickler_steps[i]);

                //Serial.print("Measurements: ");
                //Serial.println(config.trickler_measurements[i], DEC);
                avrWeight = config.trickler_measurements[i];

                String infoText = "";
                infoText += "W" + String(config.trickler_weight[i], 3) + " ";
                infoText += "ST" + String(config.trickler_steps[i]) + " ";
                infoText += "SP" + String(config.trickler_speed[i]) + " ";
                infoText += "M" + String(config.trickler_measurements[i]);
                labelInfo.drawButton(false, infoText);
                break;
              }
            }
          }
          if (config.mode == logger && (abs(weight - targetWeight) > 0.0001)) {
            //Serial.println("Log to File");
            if (String(path).length() > 0) {
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
            stepperX.disable();
          }
        }

        if ((weight <= 0.0001) && finished) {
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
