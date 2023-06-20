#define  CHX                     0x90
#define CHY                     0xD0
#define TFT_COLOR_RED                   0xF800
#define TFT_COLOR_GREEN                 0x07E0
#define TFT_COLOR_BLUE                  0x001F
#define TFT_COLOR_BLACK                 0x0000
#define TFT_COLOR_WHITE                 0xFFFF
#define TFT_COLOR_YELLOW                0xFFE0

#define TFT_LCD_CS_H        digitalWrite(LCD_CS, HIGH)
#define TFT_LCD_CS_L        digitalWrite(LCD_CS, LOW)

#define TFT_TOUCH_CS_H      digitalWrite(TOUCH_CS, HIGH)
#define TFT_TOUCH_CS_L      digitalWrite(TOUCH_CS, LOW)

#define LCD_BLK_ON          digitalWrite(LCD_EN, LOW)
#define LCD_BLK_OFF         digitalWrite(LCD_EN, HIGH)



void tft_TS35_init() {
  Serial.println("tft_TS35_init()");
  pinMode(LCD_EN, OUTPUT);
  LCD_BLK_ON;

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(tft.color565(0X1A, 0X1A, 0X1A));
  tft.initDMA();
  delay(100);
}
