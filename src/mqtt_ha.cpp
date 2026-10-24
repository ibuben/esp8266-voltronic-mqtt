#include "mqtt_ha.h"
#include "settings.h"
#include "config.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient net;
static PubSubClient mqtt(net);
static uint32_t lastReconnectMs = 0;
static bool pv2Announced = false;
static char jsonBuf[1024];

struct HaSensor {
  const char *id;
  const char *name;
  const char *key;
  const char *unit;
  const char *devClass;
  const char *stateClass;
};

static const HaSensor kSensors[] = {
    {"grid_voltage", "Grid voltage", "grid_voltage_v", "V", "voltage", "measurement"},
    {"grid_frequency", "Grid frequency", "grid_frequency_hz", "Hz", nullptr, "measurement"},
    {"output_voltage", "Output voltage", "output_voltage_v", "V", "voltage", "measurement"},
    {"output_frequency", "Output frequency", "output_frequency_hz", "Hz", nullptr, "measurement"},
    {"apparent_power", "Apparent power", "output_apparent_power_va", "VA", nullptr, "measurement"},
    {"active_power", "Active power", "output_active_power_w", "W", "power", "measurement"},
    {"load_percent", "Load", "output_load_percent", "%", nullptr, "measurement"},
    {"bus_voltage", "Bus voltage", "bus_voltage_v", "V", "voltage", "measurement"},
    {"battery_voltage", "Battery voltage", "battery_voltage_v", "V", "voltage", "measurement"},
    {"battery_charge_a", "Battery charge current", "battery_charge_current_a", "A", "current", "measurement"},
    {"battery_capacity", "Battery capacity", "battery_capacity_pct", "%", "battery", "measurement"},
    {"heatsink_temp", "Inverter temp", "inverter_heatsink_temp_c", "°C", "temperature", "measurement"},
    {"pv_charge_a", "PV charge current", "pv_charge_current_a", "A", "current", "measurement"},
    {"pv_voltage", "PV voltage", "pv_voltage_v", "V", "voltage", "measurement"},
    {"pv_power", "PV power", "pv_power_w", "W", "power", "measurement"},
    {"scc_batt_voltage", "SCC battery voltage", "scc_battery_voltage_v", "V", "voltage", "measurement"},
    {"batt_discharge_a", "Battery discharge current", "battery_discharge_current_a", "A", "current", "measurement"},
    {"status_bits", "Status bits", "status_bits", nullptr, nullptr, nullptr},
    {"mode", "Mode", "mode", nullptr, nullptr, nullptr},
    {"protocol", "Protocol", "protocol", nullptr, nullptr, nullptr},
};

static const HaSensor kPv2Sensors[] = {
    {"pv2_current", "PV2 current", "pv2_current_a", "A", "current", "measurement"},
    {"pv2_voltage", "PV2 voltage", "pv2_voltage_v", "V", "voltage", "measurement"},
    {"pv2_power", "PV2 power", "pv2_power_w", "W", "power", "measurement"},
    {"pv_total_power", "PV total power", "pv_total_power_w", "W", "power", "measurement"},
};

static void publishSensors(const HaSensor *sensors, size_t count) {
  for (size_t i = 0; i < count; i++) {
    const HaSensor &s = sensors[i];
    StaticJsonDocument<512> doc;
    char uid[64];
    snprintf(uid, sizeof(uid), "%s_%s", settings.device_id, s.id);
    doc["name"] = s.name;
    doc["unique_id"] = uid;
    doc["object_id"] = uid;
    doc["state_topic"] = settings.mqtt_topic;
    JsonArray av = doc.createNestedArray("availability");
    JsonObject a0 = av.createNestedObject();
    a0["topic"] = settings.mqtt_status;
    JsonObject dev = doc.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(settings.device_id);
    dev["name"] = settings.device_name;
    dev["manufacturer"] = "Voltronic";
    dev["model"] = "Axpert Ultra";
    dev["sw_version"] = FW_VERSION;
    char tmpl[48];
    snprintf(tmpl, sizeof(tmpl), "{{ value_json.%s }}", s.key);
    doc["value_template"] = tmpl;
    if (s.unit) {
      doc["unit_of_measurement"] = s.unit;
    }
    if (s.devClass) {
      doc["device_class"] = s.devClass;
    }
    if (s.stateClass) {
      doc["state_class"] = s.stateClass;
    }

    serializeJson(doc, jsonBuf, sizeof(jsonBuf));
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/sensor/%s_%s/config", DEFAULT_HA_PREFIX, settings.device_id, s.id);
    mqtt.publish(topic, jsonBuf, true);
    delay(20);
    yield();
  }
}

static void sendDiscovery() {
  if (!settings.ha_discovery || !mqtt.connected()) {
    return;
  }
  publishSensors(kSensors, sizeof(kSensors) / sizeof(kSensors[0]));
}

static void sendPv2Discovery() {
  if (!settings.ha_discovery || !mqtt.connected()) {
    return;
  }
  publishSensors(kPv2Sensors, sizeof(kPv2Sensors) / sizeof(kPv2Sensors[0]));
}

static bool tryConnect() {
  if (settings.mqtt_host[0] == '\0') {
    return false;
  }

  mqtt.setServer(settings.mqtt_host, settings.mqtt_port);
  mqtt.setKeepAlive(30);
  mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE);

  bool ok;
  if (settings.mqtt_user[0]) {
    ok = mqtt.connect(settingsClientId(), settings.mqtt_user, settings.mqtt_pass,
                      settings.mqtt_status, 0, true, "offline");
  } else {
    ok = mqtt.connect(settingsClientId(), nullptr, nullptr, settings.mqtt_status, 0, true, "offline");
  }
  if (!ok) {
    Serial.printf("MQTT fail rc=%d\n", mqtt.state());
    return false;
  }

  Serial.println(F("MQTT connected"));
  mqtt.publish(settings.mqtt_status, "online", true);
  sendDiscovery();
  pv2Announced = false;
  return true;
}

void mqttSetup() {
  mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE);
  lastReconnectMs = millis() - MQTT_RECONNECT_MS;
}

void mqttForceReconnect() {
  if (mqtt.connected()) {
    mqtt.publish(settings.mqtt_status, "offline", true);
    mqtt.disconnect();
  }
  lastReconnectMs = 0;
}

void mqttLoop() {
  if (settings.mqtt_host[0] == '\0' || WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  const uint32_t now = millis();
  if (now - lastReconnectMs < MQTT_RECONNECT_MS) {
    return;
  }
  lastReconnectMs = now;
  tryConnect();
}

bool mqttConnected() {
  return mqtt.connected();
}

void mqttPublishOnline() {
  if (mqtt.connected()) {
    mqtt.publish(settings.mqtt_status, "online", true);
  }
}

// float 49.9 в JSON становится 49.90000153, поэтому округляем в double.
static double roundTo(float v, int digits) {
  const double k = digits == 2 ? 100.0 : 10.0;
  return round(static_cast<double>(v) * k) / k;
}

bool mqttPublishTelemetry(const Telemetry &t) {
  if (!mqtt.connected() || !t.valid) {
    return false;
  }
  if (t.pv2_state == PV2_OK && !pv2Announced) {
    sendPv2Discovery();
    pv2Announced = true;
  }

  StaticJsonDocument<1024> doc;
  doc["grid_voltage_v"] = roundTo(t.grid_voltage_v, 1);
  doc["grid_frequency_hz"] = roundTo(t.grid_frequency_hz, 1);
  doc["output_voltage_v"] = roundTo(t.output_voltage_v, 1);
  doc["output_frequency_hz"] = roundTo(t.output_frequency_hz, 1);
  doc["output_apparent_power_va"] = t.output_apparent_power_va;
  doc["output_active_power_w"] = t.output_active_power_w;
  doc["output_load_percent"] = t.output_load_percent;
  doc["bus_voltage_v"] = t.bus_voltage_v;
  doc["battery_voltage_v"] = roundTo(t.battery_voltage_v, 2);
  doc["battery_charge_current_a"] = t.battery_charge_current_a;
  doc["battery_capacity_pct"] = t.battery_capacity_pct;
  doc["inverter_heatsink_temp_c"] = t.inverter_heatsink_temp_c;
  doc["pv_charge_current_a"] = t.pv_charge_current_a;
  doc["pv_voltage_v"] = roundTo(t.pv_voltage_v, 1);
  doc["scc_battery_voltage_v"] = roundTo(t.scc_battery_voltage_v, 2);
  doc["battery_discharge_current_a"] = t.battery_discharge_current_a;
  doc["status_bits"] = t.status_bits;
  doc["pv_power_w"] = t.pv_power_w;
  if (t.pv2_state == PV2_OK) {
    doc["pv2_current_a"] = roundTo(t.pv2_current_a, 1);
    doc["pv2_voltage_v"] = roundTo(t.pv2_voltage_v, 1);
    doc["pv2_power_w"] = t.pv2_power_w;
    doc["pv_total_power_w"] = t.pv_power_w + t.pv2_power_w;
  }
  if (t.protocol[0]) {
    doc["protocol"] = t.protocol;
  }
  if (t.mode[0]) {
    doc["mode"] = t.mode;
  }
  doc["ts"] = t.ts;

  serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  const bool ok = mqtt.publish(settings.mqtt_topic, jsonBuf, false);
  if (ok) {
    mqtt.publish(settings.mqtt_status, "online", true);
  }
  return ok;
}
