#include "settings.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

Settings settings;
static char apName[24];
static char clientId[24];

void settingsDefaults() {
  settings.mqtt_host[0] = '\0';
  settings.mqtt_port = DEFAULT_MQTT_PORT;
  settings.mqtt_user[0] = '\0';
  settings.mqtt_pass[0] = '\0';
  strlcpy(settings.mqtt_topic, DEFAULT_MQTT_TOPIC, sizeof(settings.mqtt_topic));
  strlcpy(settings.mqtt_status, DEFAULT_MQTT_STATUS, sizeof(settings.mqtt_status));
  strlcpy(settings.device_name, DEFAULT_DEVICE_NAME, sizeof(settings.device_name));
  strlcpy(settings.device_id, DEFAULT_DEVICE_ID, sizeof(settings.device_id));
  settings.poll_interval = DEFAULT_POLL_SEC;
  settings.ha_discovery = true;
  settings.inv_rx = PIN_INV_RX;
  settings.inv_tx = PIN_INV_TX;
  settings.inv_baud = INV_BAUD;
  settings.lang = LANG_EN;
}

void settingsClamp() {
  if (settings.mqtt_port == 0) {
    settings.mqtt_port = DEFAULT_MQTT_PORT;
  }
  if (settings.poll_interval < POLL_MIN_SEC) {
    settings.poll_interval = POLL_MIN_SEC;
  }
  if (settings.poll_interval > POLL_MAX_SEC) {
    settings.poll_interval = POLL_MAX_SEC;
  }
  if (settings.mqtt_topic[0] == '\0') {
    strlcpy(settings.mqtt_topic, DEFAULT_MQTT_TOPIC, sizeof(settings.mqtt_topic));
  }
  if (settings.mqtt_status[0] == '\0') {
    strlcpy(settings.mqtt_status, DEFAULT_MQTT_STATUS, sizeof(settings.mqtt_status));
  }
  if (settings.device_name[0] == '\0') {
    strlcpy(settings.device_name, DEFAULT_DEVICE_NAME, sizeof(settings.device_name));
  }
  if (settings.device_id[0] == '\0') {
    strlcpy(settings.device_id, DEFAULT_DEVICE_ID, sizeof(settings.device_id));
  }
  if (!invPinAllowed(settings.inv_rx)) {
    settings.inv_rx = PIN_INV_RX;
  }
  if (!invPinAllowed(settings.inv_tx) || settings.inv_tx == settings.inv_rx) {
    settings.inv_tx = (settings.inv_rx == PIN_INV_TX) ? PIN_INV_RX : PIN_INV_TX;
  }
  if (!invBaudAllowed(settings.inv_baud)) {
    settings.inv_baud = INV_BAUD;
  }
  if (settings.lang != LANG_RU) {
    settings.lang = LANG_EN;
  }
}

bool settingsLoad() {
  settingsDefaults();
  if (!LittleFS.exists(CFG_PATH)) {
    return false;
  }

  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) {
    return false;
  }

  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return false;
  }

  strlcpy(settings.mqtt_host, doc["mqtt_host"] | "", sizeof(settings.mqtt_host));
  settings.mqtt_port = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
  strlcpy(settings.mqtt_user, doc["mqtt_user"] | "", sizeof(settings.mqtt_user));
  strlcpy(settings.mqtt_pass, doc["mqtt_pass"] | "", sizeof(settings.mqtt_pass));
  strlcpy(settings.mqtt_topic, doc["mqtt_topic"] | DEFAULT_MQTT_TOPIC, sizeof(settings.mqtt_topic));
  strlcpy(settings.mqtt_status, doc["mqtt_status"] | DEFAULT_MQTT_STATUS, sizeof(settings.mqtt_status));
  strlcpy(settings.device_name, doc["device_name"] | DEFAULT_DEVICE_NAME, sizeof(settings.device_name));
  strlcpy(settings.device_id, doc["device_id"] | DEFAULT_DEVICE_ID, sizeof(settings.device_id));
  settings.poll_interval = doc["poll_interval"] | DEFAULT_POLL_SEC;
  settings.ha_discovery = doc["ha_discovery"] | true;
  settings.inv_rx = doc["inv_rx"] | PIN_INV_RX;
  settings.inv_tx = doc["inv_tx"] | PIN_INV_TX;
  settings.inv_baud = doc["inv_baud"] | (uint32_t)INV_BAUD;
  settings.lang = doc["lang"] | (uint8_t)LANG_EN;
  settingsClamp();
  return true;
}

bool settingsSave() {
  settingsClamp();
  StaticJsonDocument<768> doc;
  doc["mqtt_host"] = settings.mqtt_host;
  doc["mqtt_port"] = settings.mqtt_port;
  doc["mqtt_user"] = settings.mqtt_user;
  doc["mqtt_pass"] = settings.mqtt_pass;
  doc["mqtt_topic"] = settings.mqtt_topic;
  doc["mqtt_status"] = settings.mqtt_status;
  doc["device_name"] = settings.device_name;
  doc["device_id"] = settings.device_id;
  doc["poll_interval"] = settings.poll_interval;
  doc["ha_discovery"] = settings.ha_discovery;
  doc["inv_rx"] = settings.inv_rx;
  doc["inv_tx"] = settings.inv_tx;
  doc["inv_baud"] = settings.inv_baud;
  doc["lang"] = settings.lang;

  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) {
    return false;
  }
  serializeJson(doc, f);
  f.close();
  return true;
}

bool settingsBegin() {
  if (!LittleFS.begin()) {
    LittleFS.format();
    if (!LittleFS.begin()) {
      settingsDefaults();
      return false;
    }
  }
  settingsLoad();
  return true;
}

const char *settingsApName() {
  snprintf(apName, sizeof(apName), "%s-%04x", AP_NAME_PREFIX, (unsigned)(ESP.getChipId() & 0xFFFF));
  return apName;
}

const char *settingsClientId() {
  snprintf(clientId, sizeof(clientId), "voltronic-%06x", (unsigned)(ESP.getChipId() & 0xFFFFFF));
  return clientId;
}
