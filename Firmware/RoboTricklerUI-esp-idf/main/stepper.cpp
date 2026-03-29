#include "config.h"
#include "arduino_compat.h"
#include "stepper_driver.h"
#include "shift_register.h"
#include "pindef.h"

#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "stepper";

#define MOTOR_STEPS 200
#define ACCEL       1000
#define STEPPER_TASK_STACK 4096
#define STEPPER_TASK_PRIO  2
#define STEPPER_QUEUE_LEN  1

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

typedef struct {
    int stepperNum;
    int steps;
    bool oscillate;
    bool reverse;
    bool acceleration;
    TaskHandle_t requester;
} StepperRequest;

static QueueHandle_t s_stepper_queue = NULL;
static TaskHandle_t s_stepper_task_handle = NULL;
static volatile bool s_stepper_stop_requested = false;

static StepperDriver *get_stepper(int stepperNum) {
    if (stepperNum == 1) return &stepper1;
    if (stepperNum == 2) return &stepper2;
    return NULL;
}

static void step_execute_request(const StepperRequest &req) {
    StepperDriver *driver = get_stepper(req.stepperNum);
    if (driver == NULL) {
        ESP_LOGW(TAG, "Invalid stepper number: %d", req.stepperNum);
        return;
    }

    if (s_stepper_stop_requested) {
        return;
    }

    driver->enable();

    if (req.acceleration) {
        driver->setSpeedProfile(StepperDriver::LINEAR_SPEED, ACCEL, 32767);
    } else {
        driver->setSpeedProfile(StepperDriver::CONSTANT_SPEED, ACCEL, 32767);
    }

    int steps = req.steps * config.microsteps;

    if (steps > 10 && req.oscillate) {
        const int forward  = steps / 2;
        const int backward = -(steps / 2);
        driver->move(forward);
        if (!s_stepper_stop_requested && !driver->stopRequested()) {
            driver->move(backward);
        }
    } else {
        if (req.reverse) {
            steps = -steps;
        }
        driver->move(steps);
    }

    driver->disable();
}

static void stepper_task(void *arg) {
    (void)arg;
    StepperRequest req = {};

    while (true) {
        if (xQueueReceive(s_stepper_queue, &req, portMAX_DELAY) == pdTRUE) {
            s_stepper_stop_requested = false;
            step_execute_request(req);
            if (req.requester != NULL) {
                xTaskNotifyGive(req.requester);
            }
        }
    }
}

static void ensure_stepper_task(void) {
    if (s_stepper_queue == NULL) {
        s_stepper_queue = xQueueCreate(STEPPER_QUEUE_LEN, sizeof(StepperRequest));
    }

    if (s_stepper_task_handle == NULL && s_stepper_queue != NULL) {
        xTaskCreatePinnedToCore(stepper_task, "stepperTask",
                                STEPPER_TASK_STACK, NULL, STEPPER_TASK_PRIO,
                                &s_stepper_task_handle, 1);
    }
}

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
    ensure_stepper_task();
    setStepperSpeed(1, 100);
    setStepperSpeed(2, 100);
    stepper1.disable();
    stepper2.disable();
}

// ============================================================
// step — perform a stepper movement (blocking)
// ============================================================
void step(int stepperNum, int steps, bool oscillate, bool reverse, bool acceleration) {
    ensure_stepper_task();
    if (s_stepper_queue == NULL) {
        ESP_LOGE(TAG, "Stepper queue not available");
        return;
    }

    const StepperRequest req = {
        .stepperNum = stepperNum,
        .steps = steps,
        .oscillate = oscillate,
        .reverse = reverse,
        .acceleration = acceleration,
        .requester = xTaskGetCurrentTaskHandle(),
    };

    if (xQueueSend(s_stepper_queue, &req, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue step request");
        return;
    }

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void stopStepperMotion() {
    s_stepper_stop_requested = true;
    stepper1.stop();
    stepper2.stop();
    stepper1.disable();
    stepper2.disable();
}
