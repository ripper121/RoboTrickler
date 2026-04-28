idf_component_register(
    SRCS "rs232.c"
    INCLUDE_DIRS "."
    REQUIRES config esp_driver_gpio esp_driver_uart log
)
