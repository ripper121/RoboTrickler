idf_component_register(
    SRCS "profile.c"
    INCLUDE_DIRS "."
    REQUIRES actuator espressif__cjson log storage_utils
)
