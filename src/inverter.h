#pragma once

#include <Arduino.h>

enum Pv2State : uint8_t {
  PV2_NO_REPLY = 0,
  PV2_OK = 1,
  PV2_NAK = 2,
};

struct Telemetry {
  bool valid;
  float grid_voltage_v;
  float grid_frequency_hz;
  float output_voltage_v;
  float output_frequency_hz;
  int output_apparent_power_va;
  int output_active_power_w;
  int output_load_percent;
  int bus_voltage_v;
  float battery_voltage_v;
  int battery_charge_current_a;
  int battery_capacity_pct;
  int inverter_heatsink_temp_c;
  int pv_charge_current_a;
  float pv_voltage_v;
  float scc_battery_voltage_v;
  int battery_discharge_current_a;
  char status_bits[17];
  int pv_power_w;
  uint8_t pv2_state;
  float pv2_current_a;
  float pv2_voltage_v;
  int pv2_power_w;
  char mode[16];
  char protocol[16];
  uint32_t ts;
  uint32_t last_ok_ms;
  uint16_t fail_count;
};

class Inverter {
 public:
  bool begin();
  bool queryProtocol(char *out, size_t outLen);
  bool poll(Telemetry &t);

  char lastRaw[160] = {};
  uint16_t lastRawLen = 0;
  uint32_t rxTotal = 0;

 private:
  void keepLastRaw(const char *raw, size_t n);
  uint16_t crc16(const uint8_t *data, size_t len) const;
  uint8_t escapeCrc(uint8_t b) const;
  void sendCommand(const char *cmd);
  bool readResponse(char *raw, size_t rawLen, uint32_t timeoutMs);
  void decodeResponse(char *resp);
  bool parseQpigs(const char *decoded, Telemetry &t);
  void queryPv2(Telemetry &t);
  const char *mapMode(const char *resp);
};

extern Inverter inverter;
