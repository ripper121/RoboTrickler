idf_component_register(
    SRCS "shift595.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_rom
        freertos
        log
)
