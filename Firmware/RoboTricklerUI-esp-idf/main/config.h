#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>

// ============================================================
// Global configuration struct (mirrors the Arduino Config)
// ============================================================
struct Config {
    // WiFi
    char wifi_ssid[64];
    char wifi_psk[64];
    char IPStatic[15];
    char IPGateway[15];
    char IPSubnet[15];
    char IPDns[15];

    // Mode & behaviour
    char mode[9];        // "trickler" or "logger"
    char beeper[6];      // "done", "button", "both"
    bool debugLog;
    bool fwCheck;

    // Scale
    float targetWeight;
    char scale_protocol[32];
    int  scale_baud;
    char scale_customCode[32];

    // Profile management
    char profile[32];
    int  log_measurements;
    int  microsteps;

    // PID parameters
    float pidThreshold;
    long  pidStepMin;
    long  pidStepMax;
    int   pidSpeed;
    int   pidConMeasurements;
    int   pidAggMeasurements;
    float pidTolerance;
    float pidAlarmThreshold;
    bool  pidOscillate;
    bool  pidReverse;
    bool  pidAcceleration;
    uint8_t pidConsNum;
    float pidConsKp;
    float pidConsKi;
    float pidConsKd;
    uint8_t pidAggNum;
    float pidAggKp;
    float pidAggKi;
    float pidAggKd;

    // Profile data (up to 16 steps)
    uint8_t profile_num[16];
    float   profile_weight[16];
    double  profile_stepsPerUnit;
    float   profile_tolerance;
    float   profile_alarmThreshold;
    int     profile_measurements[16];
    long    profile_steps[16];
    int     profile_speed[16];
    bool    profile_oscillate[16];
    bool    profile_reverse[16];
    bool    profile_acceleration[16];
    int     profile_count;
};

// ============================================================
// Global variables — defined in main.cpp, declared extern here
// ============================================================
#ifdef __cplusplus
extern Config config;

extern float weight;
extern float addWeight;
extern int   weightCounter;
extern int   measurementCount;
extern float newData;
extern bool  running;
extern bool  finished;
extern bool  firstThrow;
extern int   logCounter;
extern std::string path;
extern bool  restart_now;
extern std::string unit;
extern int   dec_places;
extern float lastWeight;

extern bool  WIFI_AKTIVE;
extern bool  PID_AKTIVE;

extern std::string infoMessagBuff[14];
extern std::string profileListBuff[32];
extern uint8_t     profileListCount;

#define MAX_TARGET_WEIGHT 999
#define FW_VERSION 3.00f

// PID I/O floats (defined in main.cpp)
extern float pid_input;
extern float pid_output;

#endif // __cplusplus
