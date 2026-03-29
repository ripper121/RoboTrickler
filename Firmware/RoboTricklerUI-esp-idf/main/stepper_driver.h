#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================
// Stepper driver — port of BasicStepperDriver + A4988
// Uses the shift register (sr_set) instead of direct GPIO.
// ============================================================

#define PIN_UNCONNECTED -1
#define IS_CONNECTED(pin) ((pin) != PIN_UNCONNECTED)

// Step pulse timing constants (microseconds)
#define STEP_HIGH_MIN_US 1
#define WAKEUP_TIME_US   1000

// Calculate step pulse duration in microseconds
// 60[s/min] * 1000000[us/s] / microsteps / steps / rpm
#define STEP_PULSE_US(steps, microsteps, rpm) \
    (60.0f * 1000000L / (steps) / (microsteps) / (rpm))

#ifndef MIN_YIELD_MICROS
#define MIN_YIELD_MICROS 50
#endif

class StepperDriver {
public:
    enum Mode  { CONSTANT_SPEED, LINEAR_SPEED };
    enum State { STOPPED, ACCELERATING, CRUISING, DECELERATING };

    struct Profile {
        Mode  mode  = CONSTANT_SPEED;
        short accel = 1000;
        short decel = 1000;
    };

    StepperDriver(short steps, short dir_pin, short step_pin);
    StepperDriver(short steps, short dir_pin, short step_pin, short enable_pin);

    void  begin(float rpm = 60, short microsteps = 1);
    void  setRPM(float rpm);
    float getRPM() const { return _rpm; }

    short setMicrostep(short microsteps);
    short getMicrostep() const { return _microsteps; }

    void  setSpeedProfile(Mode mode, short accel = 1000, short decel = 1000);
    void  setEnableActiveState(short state);

    void  move(long steps);
    void  rotate(long deg);
    void  rotate(double deg);

    void  startMove(long steps, long time_us = 0);
    void  startRotate(long deg);
    void  startRotate(double deg);
    long  nextAction();
    void  startBrake();
    long  stop();

    State getCurrentState() const;
    long  getStepsCompleted()  const { return _step_count; }
    long  getStepsRemaining()  const { return _steps_remaining; }
    int   getDirection()       const { return (_dir_state == 1) ? 1 : -1; }
    long  getTimeForMove(long steps);

    long  calcStepsForRotation(long deg) const {
        return deg * _motor_steps * (long)_microsteps / 360;
    }
    long  calcStepsForRotation(double deg) const {
        return (long)(deg * _motor_steps * _microsteps / 360.0);
    }

    void  enable();
    void  disable();

    // Beep via shift register beeper pin
    void  beep(unsigned long time_ms);

private:
    short _motor_steps;
    short _dir_pin;
    short _step_pin;
    short _enable_pin;
    short _enable_active_state = 1; // HIGH
    short _microsteps = 1;
    float _rpm = 0;

    struct Profile _profile;

    long  _step_count        = 0;
    long  _steps_remaining   = 0;
    long  _steps_to_cruise   = 0;
    long  _steps_to_brake    = 0;
    long  _step_pulse        = 0;
    long  _cruise_step_pulse = 0;
    long  _rest              = 0;
    short _dir_state         = 0;

    uint32_t _last_action_end     = 0;
    uint32_t _next_action_interval = 0;

    void _calc_step_pulse();
    void _delay_micros(uint32_t us, uint32_t start_us = 0);

    short _get_max_microstep() const { return 16; } // A4988 max
};
