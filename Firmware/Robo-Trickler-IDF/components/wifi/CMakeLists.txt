idf_component_register(
    SRCS "wifi.c"
    INCLUDE_DIRS "."
    REQUIRES config esp_wifi esp_netif esp_event freertos log lwip nvs_flash
)
