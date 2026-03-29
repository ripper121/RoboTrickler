#include "stepper_driver.h"
#include "shift_register.h"
#include "pindef.h"
#include "arduino_compat.h"   // micros(), delay()

#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================
// Constructor
// ============================================================
StepperDriver::StepperDriver(short steps, short dir_pin, short step_pin)
    : StepperDriver(steps, dir_pin, step_pin, PIN_UNCONNECTED) {}

StepperDriver::StepperDriver(short steps, short dir_pin, short step_pin, short enable_pin)
    : _motor_steps(steps), _dir_pin(dir_pin), _step_pin(step_pin), _enable_pin(enable_pin) {
    _step_count = _steps_remaining = _dir_state = 0;
    _steps_to_cruise = _steps_to_brake = 0;
    _step_pulse = _cruise_step_pulse = _rest = 0;
}

// ============================================================
// begin — sets RPM and microsteps, enables motor
// ============================================================
void StepperDriver::begin(float rpm, short microsteps) {
    sr_set(_dir_pin,  1);  // HIGH = forward
    sr_set(_step_pin, 0);

    if (IS_CONNECTED(_enable_pin)) {
        disable();
    }
    _rpm = rpm;
    setMicrostep(microsteps);
    enable();
}

void StepperDriver::setRPM(float rpm) {
    if (_rpm == 0) { begin(rpm, _microsteps); return; }
    _rpm = rpm;
}

short StepperDriver::setMicrostep(short microsteps) {
    for (short ms = 1; ms <= _get_max_microstep(); ms <<= 1) {
        if (microsteps == ms) { _microsteps = microsteps; break; }
    }
    return _microsteps;
}

void StepperDriver::setSpeedProfile(Mode mode, short accel, short decel) {
    _profile.mode  = mode;
    _profile.accel = accel;
    _profile.decel = decel;
}

void StepperDriver::setEnableActiveState(short state) {
    _enable_active_state = state;
}

void StepperDriver::enable() {
    if (IS_CONNECTED(_enable_pin)) {
        sr_set(_enable_pin, _enable_active_state);
    }
    // Brief settling time
    uint32_t start = micros();
    while (micros() - start < 2) {}
}

void StepperDriver::disable() {
    if (IS_CONNECTED(_enable_pin)) {
        sr_set(_enable_pin, (_enable_active_state == 1) ? 0 : 1);
    }
}

// ============================================================
// Busy-wait delay in microseconds
// ============================================================
void StepperDriver::_delay_micros(uint32_t us, uint32_t start_us) {
    if (us == 0) return;
    if (!start_us) start_us = micros();
    if (us > MIN_YIELD_MICROS) taskYIELD();
    while (micros() - start_us < us) {}
}

// ============================================================
// Move blocking
// ============================================================
void StepperDriver::move(long steps) {
    startMove(steps);
    while (nextAction()) {}
}

void StepperDriver::rotate(long deg) {
    move(calcStepsForRotation(deg));
}

void StepperDriver::rotate(double deg) {
    move(calcStepsForRotation(deg));
}

// ============================================================
// startMove — calculate motion profile parameters
// ============================================================
void StepperDriver::startMove(long steps, long time_us) {
    float speed;
    _dir_state = (steps >= 0) ? 1 : 0;
    _last_action_end = 0;
    _steps_remaining = labs(steps);
    _step_count = _rest = 0;

    switch (_profile.mode) {
    case LINEAR_SPEED: {
        speed = _rpm * _motor_steps / 60.0f;
        if (time_us > 0) {
            float t = time_us / 1e6f;
            float d = (float)_steps_remaining / _microsteps;
            float a2 = 1.0f / _profile.accel + 1.0f / _profile.decel;
            float sc = t * t - 2 * a2 * d;
            if (sc >= 0) {
                float v = (t - sqrtf(sc)) / a2;
                if (v < speed) speed = v;
            }
        }
        _steps_to_cruise = (long)(_microsteps * (speed * speed / (2 * _profile.accel)));
        _steps_to_brake  = (long)(_steps_to_cruise * _profile.accel / _profile.decel);
        if (_steps_remaining < _steps_to_cruise + _steps_to_brake) {
            _steps_to_cruise = _steps_remaining * _profile.decel / (_profile.accel + _profile.decel);
            _steps_to_brake  = _steps_remaining - _steps_to_cruise;
        }
        _step_pulse        = (long)(1e6f * 0.676f * sqrtf(2.0f / _profile.accel / _microsteps));
        _cruise_step_pulse = (long)(1e6f / speed / _microsteps);
        break;
    }
    case CONSTANT_SPEED:
    default:
        _steps_to_cruise = 0;
        _steps_to_brake  = 0;
        _step_pulse = _cruise_step_pulse = (long)STEP_PULSE_US(_motor_steps, _microsteps, _rpm);
        if (time_us > (long)(_steps_remaining * _step_pulse)) {
            _step_pulse = time_us / _steps_remaining;
        }
        break;
    }
}

void StepperDriver::startRotate(long deg)   { startMove(calcStepsForRotation(deg)); }
void StepperDriver::startRotate(double deg) { startMove(calcStepsForRotation(deg)); }

void StepperDriver::startBrake() {
    switch (getCurrentState()) {
    case CRUISING:      _steps_remaining = _steps_to_brake; break;
    case ACCELERATING:  _steps_remaining = _step_count * _profile.accel / _profile.decel; break;
    default: break;
    }
}

long StepperDriver::stop() {
    long rem = _steps_remaining;
    _steps_remaining = 0;
    return rem;
}

// ============================================================
// _calc_step_pulse — update timing for next step (linear ramp)
// ============================================================
void StepperDriver::_calc_step_pulse() {
    if (_steps_remaining <= 0) return;
    _steps_remaining--;
    _step_count++;

    if (_profile.mode == LINEAR_SPEED) {
        switch (getCurrentState()) {
        case ACCELERATING:
            if (_step_count < _steps_to_cruise) {
                long denom = 4 * _step_count + 1;
                _step_pulse -= (2 * _step_pulse + _rest) / denom;
                _rest = (_step_count < _steps_to_cruise) ? (2 * _step_pulse + _rest) % denom : 0;
            } else {
                _step_pulse = _cruise_step_pulse;
            }
            break;
        case DECELERATING: {
            long denom = -4 * _steps_remaining + 1;
            _step_pulse -= (2 * _step_pulse + _rest) / denom;
            _rest = (2 * _step_pulse + _rest) % denom;
            break;
        }
        default: break;
        }
    }
}

// ============================================================
// nextAction — toggle STEP pin and return time to next action
// ============================================================
long StepperDriver::nextAction() {
    if (_steps_remaining > 0) {
        _delay_micros(_next_action_interval, _last_action_end);

        sr_set(_dir_pin,  _dir_state);
        sr_set(_step_pin, 1);

        uint32_t m = micros();
        long pulse = _step_pulse;
        _calc_step_pulse();

        // Hold STEP HIGH for minimum pulse width
        uint32_t start_high = micros();
        while (micros() - start_high < STEP_HIGH_MIN_US) {}

        sr_set(_step_pin, 0);

        _last_action_end = micros();
        uint32_t elapsed = _last_action_end - m;
        _next_action_interval = (pulse > (long)elapsed) ? (pulse - elapsed) : 1;
    } else {
        _last_action_end = 0;
        _next_action_interval = 0;
    }
    return (long)_next_action_interval;
}

StepperDriver::State StepperDriver::getCurrentState() const {
    if (_steps_remaining <= 0) return STOPPED;
    if (_steps_remaining <= _steps_to_brake) return DECELERATING;
    if (_step_count <= _steps_to_cruise)     return ACCELERATING;
    return CRUISING;
}

long StepperDriver::getTimeForMove(long steps) {
    if (steps == 0) return 0;
    switch (_profile.mode) {
    case LINEAR_SPEED: {
        startMove(steps);
        long cruise = _steps_remaining - _steps_to_cruise - _steps_to_brake;
        float speed = _rpm * _motor_steps / 60.0f;
        float t = (float)cruise / (_microsteps * speed)
                + sqrtf(2.0f * _steps_to_cruise / _profile.accel / _microsteps)
                + sqrtf(2.0f * _steps_to_brake  / _profile.decel / _microsteps);
        return (long)roundf(t * 1e6f);
    }
    default:
        return (long)(steps * STEP_PULSE_US(_motor_steps, _microsteps, _rpm));
    }
}

// ============================================================
// Beep using the shift register beeper bit
// ============================================================
void StepperDriver::beep(unsigned long time_ms) {
    sr_set(I2S_BEEPER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(time_ms));
    sr_set(I2S_BEEPER_PIN, 0);
}
