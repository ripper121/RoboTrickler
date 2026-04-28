idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server espressif__cjson log vfs espressif__mdns ui
)
