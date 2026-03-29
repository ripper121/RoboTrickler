cmake_minimum_required(VERSION 3.16)

# Path to the actual LVGL source (Arduino-Libs/lvgl)
get_filename_component(LVGL_ROOT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../Arduino-Libs/lvgl"
    ABSOLUTE)

# lv_conf.h wrapper lives inside this repo and can override selected options
get_filename_component(LV_CONF_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../lvgl_config"
    ABSOLUTE)

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_ROOT_DIR}/src/*.c")

idf_component_register(
    SRCS        ${LVGL_SOURCES}
    INCLUDE_DIRS
        "${LV_CONF_DIR}"
        "${LVGL_ROOT_DIR}"
        "${LVGL_ROOT_DIR}/src"
    REQUIRES    esp_timer
)

# LV_CONF_INCLUDE_SIMPLE means lv_conf.h is on the include path (which it is via LV_CONF_DIR)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")
