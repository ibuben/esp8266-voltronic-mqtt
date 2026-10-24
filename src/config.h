#pragma once

#include <Arduino.h>

#define FW_VERSION "1.1"

// NodeMCU v3 (ESP-12E) + RS232-TTL:
//   D1 (GPIO5)   <- TXD конвертера (TTL TX инвертора)
//   D2 (GPIO4)   -> RXD конвертера (TTL RX инвертора)
//   GND          -- GND
//   3V3          -- VCC конвертера (только 3.3V TTL, не 5V!)
// Кнопка FLASH (D3/GPIO0): удержать 3 с — портал настройки Wi-Fi.
#define PIN_INV_RX    5
#define PIN_INV_TX    4
#define PIN_CFG_BTN   0
#define PIN_LED       2

#define INV_BAUD           2400

struct InvPinOpt {
  uint8_t gpio;
  const char *label;
};

inline const InvPinOpt *invPinOptions(size_t &count) {
  static const InvPinOpt kPins[] = {
      {5, "D1 (GPIO5)"},  {4, "D2 (GPIO4)"},   {14, "D5 (GPIO14)"},
      {12, "D6 (GPIO12)"}, {13, "D7 (GPIO13)"}, {15, "D8 (GPIO15)"},
  };
  count = sizeof(kPins) / sizeof(kPins[0]);
  return kPins;
}

inline const char *invPinLabel(uint8_t gpio) {
  size_t n = 0;
  const InvPinOpt *pins = invPinOptions(n);
  for (size_t i = 0; i < n; i++) {
    if (pins[i].gpio == gpio) {
      return pins[i].label;
    }
  }
  return "?";
}

inline bool invPinAllowed(uint8_t gpio) {
  size_t n = 0;
  const InvPinOpt *pins = invPinOptions(n);
  for (size_t i = 0; i < n; i++) {
    if (pins[i].gpio == gpio) {
      return true;
    }
  }
  return false;
}

inline const uint32_t *invBaudOptions(size_t &count) {
  static const uint32_t kBauds[] = {1200, 2400, 4800, 9600, 19200};
  count = sizeof(kBauds) / sizeof(kBauds[0]);
  return kBauds;
}

inline bool invBaudAllowed(uint32_t baud) {
  size_t n = 0;
  const uint32_t *bauds = invBaudOptions(n);
  for (size_t i = 0; i < n; i++) {
    if (bauds[i] == baud) {
      return true;
    }
  }
  return false;
}
#define INV_TIMEOUT_MS     2000
#define INV_RETRY          1

#define AP_NAME_PREFIX     "voltronic"
#define PORTAL_TIMEOUT_S   180
#define WIFI_CONNECT_MS    30000
#define MQTT_RECONNECT_MS  5000
#define BTN_HOLD_MS        3000
#define CFG_PATH           "/config.json"

#define DEFAULT_MQTT_PORT       1883
#define DEFAULT_POLL_SEC        5
#define DEFAULT_MQTT_TOPIC      "home/axpert-ultra/tele"
#define DEFAULT_MQTT_STATUS     "home/axpert-ultra/status"
#define DEFAULT_DEVICE_NAME     "Axpert Ultra"
#define DEFAULT_DEVICE_ID       "axpert-ultra"
#define DEFAULT_HA_PREFIX       "homeassistant"

#define POLL_MIN_SEC  2
#define POLL_MAX_SEC  120
