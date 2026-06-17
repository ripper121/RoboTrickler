#define I2S_OUT_NUM_BITS    32

#define I2S_OUT_BCK                 GPIO_NUM_16
#define I2S_OUT_WS                  GPIO_NUM_17
#define I2S_OUT_DATA                GPIO_NUM_21
    
#define I2S_X_DISABLE_PIN               0
#define I2S_X_DIRECTION_PIN             2
#define I2S_X_STEP_PIN                  1
    
#define I2S_Y_DISABLE_PIN               0
#define I2S_Y_DIRECTION_PIN             6
#define I2S_Y_STEP_PIN                  5 
    
#define I2S_Z_DISABLE_PIN               0
#define I2S_Z_DIRECTION_PIN             4
#define I2S_Z_STEP_PIN                  3

#define SPINDLE_OUTPUT_PIN            GPIO_NUM_32

#define X_LIMIT_PIN                 GPIO_NUM_36
#define Y_LIMIT_PIN                 GPIO_NUM_35
#define Z_LIMIT_PIN                 GPIO_NUM_34

#define PROBE_PIN                   GPIO_NUM_2     

// Display (ST7796, 3.5" E32R35T/E32N35T). Pins also live in TFT_eSPI/User_Setup.h.
#define LCD_SCK				      GPIO_NUM_14
#define LCD_MISO				    GPIO_NUM_12
#define LCD_MOSI				    GPIO_NUM_13
#define LCD_RS					    GPIO_NUM_2      // DC / data-command
#define LCD_RST					    -1             // tied to ESP32 EN
#define LCD_CS					    GPIO_NUM_15
#define LCD_BL					    GPIO_NUM_27     // backlight, active HIGH

#define TOUCH_CS				    GPIO_NUM_33
#define DISPLAY_ROTATION            0
// Touch calibration for the 3.5" panel (E32R35T/E32N35T).
// The TFT_eSPI ST7796 driver sets MADCTL MV at rotation 0, so DISPLAY_ROTATION 0
// is a landscape (480x320) frame. The 5th value is a flag mask
// (bit0=swap XY, bit1=invert X, bit2=invert Y); landscape needs the swap bit.
// This panel's touch was 180 deg rotated with the vendor's swap+invert set
// (flag 7), so both invert bits are cleared (flag 1 = swap only).
#define TOUCH_CAL_DATA              {245, 3664, 259, 3471, 1}
#define I2S_BEEPER					    7

#define IIC_SCL                     GPIO_NUM_4
#define IIC_SDA                     GPIO_NUM_0
#define SCALE_RX_PIN                IIC_SCL
#define SCALE_TX_PIN                IIC_SDA

#define BLTOUCH_PWM                 GPIO_NUM_2
#define BLTOUCH_READ                GPIO_NUM_34

// SD card SPI (own bus, HSPI)
#define GRBL_SPI_SCK                        GPIO_NUM_18
#define GRBL_SPI_MISO                       GPIO_NUM_19
#define GRBL_SPI_MOSI                       GPIO_NUM_23
#define GRBL_SPI_SS                         GPIO_NUM_5
#define SDCARD_DET_PIN                      GPIO_NUM_39
#define SD_SPI_FREQ                          20000000
