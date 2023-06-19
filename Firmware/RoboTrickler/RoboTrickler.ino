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

#define SBI 1
#define GUG 0

Preferences preferences;

#define WIFI_UPDATE 0
WebServer server(80);
bool OTA_running = false;

TFT_eSPI tft = TFT_eSPI();
#define LCD_WIDTH               480
#define LCD_HEIGHT              320
#define BUTTON_H 45
ButtonWidget btnSub = ButtonWidget(&tft);
ButtonWidget btnAdd = ButtonWidget(&tft);
ButtonWidget labelTarget = ButtonWidget(&tft);
ButtonWidget btnDown = ButtonWidget(&tft);
ButtonWidget btnUp = ButtonWidget(&tft);
ButtonWidget labelSpeed = ButtonWidget(&tft);
ButtonWidget labelWeight = ButtonWidget(&tft);
ButtonWidget btnStart = ButtonWidget(&tft);
ButtonWidget btnStop = ButtonWidget(&tft);
ButtonWidget btn1 = ButtonWidget(&tft);
ButtonWidget btn10 = ButtonWidget(&tft);
ButtonWidget btn100 = ButtonWidget(&tft);
ButtonWidget labelBanner = ButtonWidget(&tft);
ButtonWidget* btn[] = {&btnSub , &labelTarget, &btnAdd, &btnStart, &btnStop, &btn1, &btn10, &btn100, &labelWeight, &btnDown, &btnUp, &labelSpeed, &labelBanner};
uint8_t buttonCount = sizeof(btn) / sizeof(btn[0]);

#define MOTOR_STEPS 200
#define RPM 200
#define MICROSTEPS 1
A4988 stepperX(MOTOR_STEPS, I2S_X_DIRECTION_PIN, I2S_X_STEP_PIN, I2S_X_DISABLE_PIN);
int addSpeed = 100;
int lastStepperSpeed = 0;
int stepperSpeed = 200;

float weight = 0.0;
float targetWeight = 0.520;
float lastTargetWeight = 0.0;
float lastWeight = 0;
float addWeight = 0.1;
int weightCounter = 0;
byte avrWeight = 2;
float newData = false;
bool running = false;
bool finished = false;
String targetWeightStr = "";
String lastTargetWeightStr = "";



void setup() {
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, IIC_SCL, IIC_SDA);

  initUpdate();

  preferences.begin("app-settings", false);
  targetWeight = preferences.getFloat("targetWeight", 0.0);
  if (targetWeight < 0 || targetWeight > 100) {
    targetWeight = 0.0;
    preferences.putFloat("targetWeight", targetWeight);
  }

  tft_TS35_init();
  initStepper();
  initButtons();
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

      if (lastStepperSpeed != stepperSpeed) {
        preferences.putInt("stepperSpeed", stepperSpeed);
      }
      lastStepperSpeed = stepperSpeed;
    }

    if (Serial1.available()) {     // 2B 20 20 31 32 37 2E 38 32 38 20 67 20 20 0D 0A
      char buff[16];
      Serial1.readBytesUntil(0x0A, buff, sizeof(buff));

      int grammH = 0;
      int grammT = 0;
      int grammO = 0;
      int milliH = 0;
      int milliT = 0;
      int milliO = 0;

      if (GUG) {
        grammH = (buff[2] > 0x30) ? (buff[2] - 0x30) : 0;
        grammT = (buff[3] > 0x30) ? (buff[3] - 0x30) : 0;
        grammO = (buff[4] > 0x30) ? (buff[4] - 0x30) : 0;
        milliH = (buff[6] > 0x30) ? (buff[6] - 0x30) : 0;
        milliT = (buff[7] > 0x30) ? (buff[7] - 0x30) : 0;
        milliO = (buff[8] > 0x30) ? (buff[8] - 0x30) : 0;
      }
      if (SBI) {
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

      Serial.print("Weight: ");
      Serial.print(weight, 3);
      Serial.print(" / Target Weight: ");
      Serial.print(targetWeight, 3);
      Serial.print(" Diff: ");
      Serial.println((weight - lastWeight), 3);
      Serial.println();

      if (lastWeight == weight) {
        weightCounter++;
        if (weightCounter > avrWeight) {
          newData = true;
          weightCounter = 0;
        }
      } else {
        weightCounter = 0;
      }
      lastWeight = weight;

      labelWeight.drawButton(false,"Diff: " + String((targetWeight - weight), 3) + " g");
    } else if (running) {
      Serial1.write(0x1B);
      Serial1.write(0x70);
      Serial1.write(0x0D);
      Serial1.write(0x0A);
    }

    if ((abs(weight - targetWeight) < 0.0005)) {
      targetWeightStr = "> " + String(targetWeight, 3) + " g <";
      if (!finished)
        stepperX.beep(500);
      finished = true;
    } else {
      targetWeightStr =  String(targetWeight, 3) + " g";
    }

    if (newData && running) {
      if (weight < targetWeight && !finished) {
        stepperX.enable();
        if (weight < (targetWeight - 1.0)) {
          Serial.println("Stepp 6x 360...");
          esp_random();
          step(random(360 * 2)); step(360 * 2); step(360 * 2);
          avrWeight = 2;
        }
        if (weight < (targetWeight - 0.5)) {
          Serial.println("Stepp 6x 360...");
          esp_random();
          step(random(360)); step(360); step(360 * 2);
          avrWeight = 2;
        } else if (weight < (targetWeight - 0.25)) {
          Serial.println("Stepp 4x 360...");
          esp_random();
          step(random(360)); step(360);
          avrWeight = 2;
        } else if (weight < (targetWeight - 0.25)) {
          Serial.println("Stepp 2x 360...");
          step(360);
          avrWeight = 2;
        } else if (weight < (targetWeight - 0.1)) {
          Serial.println("Stepp 360...");
          step(180);
          avrWeight = 2;
        } else if (weight < (targetWeight - 0.04)) {
          Serial.println("Stepp 270...");
          step(90);
          avrWeight = 2;
        } else if (weight < (targetWeight - 0.025)) {
          Serial.println("Stepp 180...");
          step(45);
          avrWeight = 5;
        } else if (weight < (targetWeight - 0.02)) {
          Serial.println("Stepp 180...");
          step(30);
          avrWeight = 5;
        } else if (weight < (targetWeight - 0.0125)) {
          Serial.println("Stepp 40...");
          step(20);
          avrWeight = 5;
        } else if (weight < (targetWeight - 0.005)) {
          Serial.println("Stepp 20...");
          step(15);
          avrWeight = 10;
        } else if (weight < (targetWeight - 0.0025)) {
          Serial.println("Stepp 20...");
          step(10);
          avrWeight = 15;
        } else {
          Serial.println("Stepp 10...");
          step(10);
          avrWeight = 20;
        }
      } else {
        stepperX.disable();
      }

      newData = false;
      while (Serial1.available()) {
        Serial1.read();
      }

      if (weight <= 0) {
        finished = false;
      }
    }
  }

  if (WIFI_UPDATE) {
    if (WiFi.status() == WL_CONNECTED) {
      server.handleClient();
      ArduinoOTA.handle();
    }
  }
}
