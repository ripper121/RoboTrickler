#ifndef ROBOTRICKLER_CONFIG_TYPES_H
#define ROBOTRICKLER_CONFIG_TYPES_H

#include <Arduino.h>
#include "constants.h"

struct ProfileActuator
{
  bool enabled;
  double unitsPerThrow;
  int unitsPerThrowSpeed;
};

struct ProfileStep
{
  byte actuator;
  float weight;
  int measurements;
  long steps;
  int speed;
  bool reverse;
};

struct Config
{
  char wifi_ssid[WIFI_CREDENTIAL_SIZE];
  char wifi_psk[WIFI_CREDENTIAL_SIZE];
  char IPStatic[IP_ADDRESS_TEXT_SIZE];
  char IPGateway[IP_ADDRESS_TEXT_SIZE];
  char IPSubnet[IP_ADDRESS_TEXT_SIZE];
  char IPDns[IP_ADDRESS_TEXT_SIZE];

  char beeper[BEEPER_TEXT_SIZE];
  char language[LANGUAGE_TEXT_SIZE];
  bool fwCheck;
  char fwUpdateUrl[FW_UPDATE_URL_SIZE];
  float targetWeight;
  char scale_protocol[SCALE_PROTOCOL_TEXT_SIZE];
  int scale_baud;
  char scale_customCode[SCALE_CUSTOM_CODE_SIZE];
  char profile[PROFILE_NAME_SIZE];

  float profile_tolerance;
  float profile_alarmThreshold;
  float profile_weightGap;
  int profile_generalMeasurements;
  ProfileActuator profile_actuator[PROFILE_ACTUATOR_SLOTS];
  ProfileStep profile_step[MAX_PROFILE_STEPS];
  int profile_count;
};

#endif
