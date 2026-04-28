idf_component_register(
    SRCS "stepper.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_rom
        freertos
        log
        shift595
)
