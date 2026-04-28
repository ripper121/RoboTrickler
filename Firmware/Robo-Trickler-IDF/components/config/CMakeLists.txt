idf_component_register(
    SRCS "config.c"
    INCLUDE_DIRS "."
    REQUIRES espressif__cjson log storage_utils
)
