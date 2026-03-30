idf_component_register(
    SRCS
        "main.cpp"
        "helper.cpp"
        "stepper.cpp"
        "csv.cpp"
        "sd_config.cpp"
        "ui_events.cpp"
        "webserver.cpp"
        "update.cpp"
        "display.cpp"
        "shift_register.cpp"
        "stepper_driver.cpp"
        "ui/ui.c"
        "ui/ui_Screen1.c"
        "ui/ui_comp_hook.c"
    INCLUDE_DIRS
        "."
        "ui"
    REQUIRES
        lvgl
        espressif__cjson
        QuickPID
        driver
        esp_driver_uart
        esp_lcd
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        esp_http_client
        mbedtls
        app_update
        fatfs
        spi_flash
        freertos
        esp_timer
        heap
        log
        nvs_flash
)

# Allow C++ exceptions and tolerate LVGL's enum flag mixing under newer C++ modes.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fexceptions
    -Wno-error=deprecated-enum-enum-conversion
)
target_link_libraries(${COMPONENT_LIB} PRIVATE -lstdc++)
