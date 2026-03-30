cmake_minimum_required(VERSION 3.16)

# ArduinoJson is header-only; just expose its include directory
idf_component_register(
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
)
