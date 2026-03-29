cmake_minimum_required(VERSION 3.16)

get_filename_component(ARDUINOJSON_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../Arduino-Libs/ArduinoJson"
    ABSOLUTE)

# ArduinoJson is header-only; just expose its include directory
idf_component_register(
    INCLUDE_DIRS "${ARDUINOJSON_DIR}/src"
)
