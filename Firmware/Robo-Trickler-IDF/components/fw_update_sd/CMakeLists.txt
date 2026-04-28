idf_component_register(
    SRCS "fw_update_sd.c"
    INCLUDE_DIRS "."
    REQUIRES log esp_common app_update
)
