idf_component_register(
    SRCS "fw_download.c"
    INCLUDE_DIRS "."
    REQUIRES config esp_http_client espressif__cjson log esp_common
)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_BINARY_DIR}")
set_source_files_properties(fw_download.c PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/build_info.h")
