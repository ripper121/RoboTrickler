#include "config.h"
#include "arduino_compat.h"
#include "stepper_driver.h"
#include "shift_register.h"
#include "pindef.h"

#include "esp_log.h"

static const char *TAG = "stepper";

#define MOTOR_STEPS 200
#define ACCEL       1000

// Two StepperDriver instances — match Arduino A4988 instantiation
// A4988(steps, dir_pin, step_pin, enable_pin)
// All pins are shift register bit indices, not GPIO numbers.
StepperDriver stepper1(MOTOR_STEPS,
                       I2S_X_DIRECTION_PIN,
                       I2S_X_STEP_PIN,
                       I2S_X_DISABLE_PIN);

StepperDriver stepper2(MOTOR_STEPS,
                       I2S_Y_DIRECTION_PIN,
                       I2S_Y_STEP_PIN,
                       I2S_Y_DISABLE_PIN);

// ============================================================
// setStepperSpeed
// ============================================================
void setStepperSpeed(int stepperNum, int _stepperSpeed) {
    if (stepperNum == 1) {
        stepper1.begin((float)_stepperSpeed, (short)config.microsteps);
        stepper1.setEnableActiveState(0);  // LOW active
    }
    if (stepperNum == 2) {
        stepper2.begin((float)_stepperSpeed, (short)config.microsteps);
        stepper2.setEnableActiveState(0);
    }
}

// ============================================================
// initStepper
// ============================================================
void initStepper() {
    ESP_LOGD(TAG, "initStepper()");
    sr_init();
    setStepperSpeed(1, 100);
    setStepperSpeed(2, 100);
    stepper1.disable();
    stepper2.disable();
}

// ============================================================
// step — perform a stepper movement (blocking)
// ============================================================
void step(int stepperNum, int steps, bool oscillate, bool reverse, bool acceleration) {
    if (stepperNum == 1) stepper1.enable();
    if (stepperNum == 2) stepper2.enable();

    if (stepperNum == 1) {
        if (acceleration)
            stepper1.setSpeedProfile(StepperDriver::LINEAR_SPEED,   ACCEL, 32767);
        else
            stepper1.setSpeedProfile(StepperDriver::CONSTANT_SPEED, ACCEL, 32767);
    }
    if (stepperNum == 2) {
        if (acceleration)
            stepper2.setSpeedProfile(StepperDriver::LINEAR_SPEED,   ACCEL, 32767);
        else
            stepper2.setSpeedProfile(StepperDriver::CONSTANT_SPEED, ACCEL, 32767);
    }

    steps = steps * config.microsteps;

    if (steps > 10 && oscillate) {
        int forward  =  steps / 2;
        int backward = -(steps / 2);
        if (stepperNum == 1) { stepper1.move(forward); stepper1.move(backward); }
        if (stepperNum == 2) { stepper2.move(forward); stepper2.move(backward); }
    } else {
        if (reverse) steps = -steps;
        if (stepperNum == 1) stepper1.move(steps);
        if (stepperNum == 2) stepper2.move(steps);
    }

    stepper1.disable();
    stepper2.disable();
}
