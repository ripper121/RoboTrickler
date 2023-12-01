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

#define LCD_SCK				      GPIO_NUM_18
#define LCD_MISO				    GPIO_NUM_19
#define LCD_MOSI				    GPIO_NUM_23
#define LCD_RS					    GPIO_NUM_33
#define LCD_EN					    GPIO_NUM_5     
#define LCD_RST					    GPIO_NUM_27     
#define LCD_CS					    GPIO_NUM_25

#define TOUCH_CS				    GPIO_NUM_26
#define I2S_BEEPER					    7

#define IIC_SCL                     GPIO_NUM_4
#define IIC_SDA                     GPIO_NUM_0

#define BLTOUCH_PWM                 GPIO_NUM_2
#define BLTOUCH_READ                GPIO_NUM_34

//sd card spi
#define GRBL_SPI_SCK 			    GPIO_NUM_14
#define GRBL_SPI_MISO 			    GPIO_NUM_12
#define GRBL_SPI_MOSI 			    GPIO_NUM_13
#define GRBL_SPI_SS 			    GPIO_NUM_15
#define SDCARD_DET_PIN 			    GPIO_NUM_39
#define GRBL_SPI_FREQ 			    40000000   
