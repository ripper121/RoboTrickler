idf_component_register(
    SRCS "actuator.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        log
        shift595
        stepper
)
