idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        config
        display
        esp_common
        esp_system
        freertos
        fw_download
        fw_update_sd
        log
        sd_card
        ui
        webserver
        wifi
)

idf_build_get_property(python PYTHON)
set(RT_BUILD_INFO_HEADER "${CMAKE_BINARY_DIR}/build_info.h")
string(CONCAT RT_BUILD_INFO_CMD
    "from pathlib import Path; import time; "
    "build_epoch=int(time.time()); "
    "build_time=time.gmtime(build_epoch); "
    "build_date=time.strftime('%Y-%m-%d', build_time); "
    "build_clock=time.strftime('%H:%M:%S', build_time); "
    "Path(r'${RT_BUILD_INFO_HEADER}').write_text("
    "f'#define RT_BUILD_DATE \\\"{build_date}\\\"\\n'"
    " + f'#define RT_BUILD_TIME \\\"{build_clock}\\\"\\n'"
    " + f'#define RT_BUILD_EPOCH {build_epoch}\\n', "
    "encoding='ascii')"
)

add_custom_target(app_build_epoch ALL
    COMMAND ${python} -c "${RT_BUILD_INFO_CMD}"
    BYPRODUCTS "${RT_BUILD_INFO_HEADER}"
    COMMENT "Generating app build epoch metadata"
    VERBATIM
)

set_source_files_properties(main.c PROPERTIES OBJECT_DEPENDS "${RT_BUILD_INFO_HEADER}")
