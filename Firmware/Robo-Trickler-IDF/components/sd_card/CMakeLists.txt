idf_component_register(
    SRCS "sd_card.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_sdspi
        esp_driver_spi
        fatfs
        log
        sdmmc
)
