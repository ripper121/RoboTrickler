idf_component_register(
    SRCS "display.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_spi
        esp_lcd
        espressif__esp_lcd_st7796
        esp_timer
        freertos
        log
        lvgl__lvgl
)
