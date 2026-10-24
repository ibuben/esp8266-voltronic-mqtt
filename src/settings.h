#pragma once

#include <Arduino.h>
#include "config.h"

enum UiLang : uint8_t {
  LANG_EN = 0,
  LANG_RU = 1,
};

struct Settings {
  char mqtt_host[64];
  uint16_t mqtt_port;
  char mqtt_user[32];
  char mqtt_pass[64];
  char mqtt_topic[64];
  char mqtt_status[64];
  char device_name[32];
  char device_id[32];
  uint16_t poll_interval;
  bool ha_discovery;
  uint8_t inv_rx;
  uint8_t inv_tx;
  uint32_t inv_baud;
  uint8_t lang;
};

extern Settings settings;

bool settingsBegin();
bool settingsLoad();
bool settingsSave();
void settingsDefaults();
void settingsClamp();
const char *settingsApName();
const char *settingsClientId();
