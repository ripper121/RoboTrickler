cmake_minimum_required(VERSION 3.16)

get_filename_component(QUICKPID_SRC
    "${CMAKE_CURRENT_LIST_DIR}/../../../Arduino-Libs/QuickPID/src"
    ABSOLUTE)

idf_component_register(
    SRCS        "${QUICKPID_SRC}/QuickPID.cpp"
    INCLUDE_DIRS
        "${QUICKPID_SRC}"
        "${CMAKE_CURRENT_LIST_DIR}"   # provides our Arduino.h stub
    REQUIRES    esp_timer freertos
)

# Define ARDUINO only for this component so its upstream source picks our stub,
# without enabling Arduino-only branches in unrelated components.
target_compile_definitions(${COMPONENT_LIB} PRIVATE -DARDUINO=200)

# Suppress some Arduino-specific warnings
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-variable
    -Wno-unused-function
)
