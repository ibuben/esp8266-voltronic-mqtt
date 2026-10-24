#include "config.h"
#include "settings.h"
#include "inverter.h"
#include "mqtt_ha.h"
#include "web.h"

#include <ESP8266WiFi.h>

static uint32_t lastPollMs = 0;
static uint32_t btnDownAt = 0;
static bool btnConsumed = false;

static void setLed(bool on) {
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

static void updateLed() {
  static uint32_t lastToggle = 0;
  static bool state = false;
  const uint32_t now = millis();

  if (mqttConnected() && telemetry.valid) {
    setLed(true);
    return;
  }

  const uint16_t period = (WiFi.status() == WL_CONNECTED) ? 800 : 200;
  if (now - lastToggle >= period) {
    lastToggle = now;
    state = !state;
    setLed(state);
  }
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  setLed(false);
  pinMode(PIN_CFG_BTN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("Voltronic MQTT / ESP8266"));
  Serial.printf("AP: %s\n", settingsApName());

  if (!settingsBegin()) {
    Serial.println(F("LittleFS init failed"));
  }
  Serial.printf("RS232 RX %s  TX %s  baud %lu\n", invPinLabel(settings.inv_rx), invPinLabel(settings.inv_tx),
                (unsigned long)settings.inv_baud);

  bool forcePortal = false;
  if (digitalRead(PIN_CFG_BTN) == LOW) {
    const uint32_t t0 = millis();
    while (digitalRead(PIN_CFG_BTN) == LOW && millis() - t0 < BTN_HOLD_MS) {
      delay(10);
      yield();
    }
    if (millis() - t0 >= BTN_HOLD_MS) {
      forcePortal = true;
      Serial.println(F("FLASH held: Wi-Fi portal"));
    }
  }

  inverter.begin();
  wifiConnectOrPortal(forcePortal);
  mqttSetup();

  if (inverter.queryProtocol(telemetry.protocol, sizeof(telemetry.protocol))) {
    Serial.printf("Inverter protocol: %s\n", telemetry.protocol);
  } else {
    Serial.println(F("Inverter QPI: no answer (check RS232 wiring/baud)"));
  }

  lastPollMs = millis() - (uint32_t)settings.poll_interval * 1000UL;
}

void loop() {
  webLoop();
  mqttLoop();
  updateLed();

  if (digitalRead(PIN_CFG_BTN) == LOW) {
    if (btnDownAt == 0) {
      btnDownAt = millis();
    } else if (!btnConsumed && millis() - btnDownAt >= BTN_HOLD_MS) {
      btnConsumed = true;
      Serial.println(F("FLASH held: Wi-Fi portal"));
      startWifiPortal(true);
    }
  } else {
    btnDownAt = 0;
    btnConsumed = false;
  }

  const uint32_t intervalMs = (uint32_t)settings.poll_interval * 1000UL;
  if (millis() - lastPollMs < intervalMs) {
    return;
  }
  lastPollMs = millis();

  char proto[16];
  strlcpy(proto, telemetry.protocol, sizeof(proto));
  if (inverter.poll(telemetry)) {
    strlcpy(telemetry.protocol, proto, sizeof(telemetry.protocol));
    mqttPublishTelemetry(telemetry);
    Serial.printf("PUB mode=%s pv=%dW load=%dW batt=%.2fV\n",
                  telemetry.mode[0] ? telemetry.mode : "-",
                  telemetry.pv_power_w,
                  telemetry.output_active_power_w,
                  telemetry.battery_voltage_v);
  } else {
    strlcpy(telemetry.protocol, proto, sizeof(telemetry.protocol));
    Serial.println(F("Inverter timeout"));
    mqttPublishOnline();
  }
}
