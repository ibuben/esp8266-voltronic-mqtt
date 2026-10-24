#include "inverter.h"
#include "config.h"
#include "settings.h"

#include <SoftwareSerial.h>

Inverter inverter;

// До 9600 включительно байт принимается и передаётся целиком при выключенных
// прерываниях, длительность бита отсчитывается по тактам CPU.
static constexpr uint32_t kBitbangMaxBaud = 9600;

static SoftwareSerial invSerial;
static bool softOn = false;
static bool bitbang = false;
static char rxBuf[192];

static uint8_t bbRxPin = 255;
static uint8_t bbTxPin = 255;
static uint32_t bbCyclesPerBit = 33333;
static uint32_t bbRxMask = 0;
static uint32_t bbTxMask = 0;
static volatile bool bbBusy = false;
static volatile uint8_t bbQ[64];
static volatile uint8_t bbHead = 0;
static volatile uint8_t bbTail = 0;

static inline uint32_t IRAM_ATTR cpuCycles() {
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
  return ccount;
}

static inline bool IRAM_ATTR bbRxHigh() {
  return (GPI & bbRxMask) != 0;
}

static void IRAM_ATTR bbRxIsr() {
  const uint32_t ps = xt_rsil(15);
  if (bbBusy) {
    xt_wsr_ps(ps);
    return;
  }
  bbBusy = true;
  const uint32_t start = cpuCycles();
  const uint32_t bit = bbCyclesPerBit;
  const uint32_t half = bit >> 1;
  while ((uint32_t)(cpuCycles() - start) < half) {
  }
  if (bbRxHigh()) {
    GPIEC = bbRxMask;
    bbBusy = false;
    xt_wsr_ps(ps);
    return;
  }

  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    const uint32_t target = start + half + bit * (i + 1);
    while ((int32_t)(cpuCycles() - target) < 0) {
    }
    if (bbRxHigh()) {
      value |= (uint8_t)(1u << i);
    }
  }
  const uint32_t stopAt = start + half + bit * 9;
  while ((int32_t)(cpuCycles() - stopAt) < 0) {
  }

  const uint8_t next = (uint8_t)((bbHead + 1) & 63);
  if (next != bbTail) {
    bbQ[bbHead] = value;
    bbHead = next;
  }
  GPIEC = bbRxMask;
  bbBusy = false;
  xt_wsr_ps(ps);
}

static void bbEnd() {
  if (bbRxPin != 255) {
    detachInterrupt(bbRxPin);
    bbRxPin = 255;
  }
  bbBusy = false;
  bbHead = 0;
  bbTail = 0;
}

static void bbBegin(uint32_t baud, uint8_t rx, uint8_t tx) {
  bbEnd();
  bbRxPin = rx;
  bbTxPin = tx;
  bbRxMask = 1UL << rx;
  bbTxMask = 1UL << tx;
  const uint32_t mhz = F_CPU / 1000000UL;
  bbCyclesPerBit = (mhz * 1000000UL + baud / 2) / baud;
  if (bbCyclesPerBit < 100) {
    bbCyclesPerBit = 100;
  }
  pinMode(bbRxPin, INPUT_PULLUP);
  pinMode(bbTxPin, OUTPUT);
  digitalWrite(bbTxPin, HIGH);
  attachInterrupt(bbRxPin, bbRxIsr, FALLING);
}

static void bbWrite(uint8_t value) {
  const uint32_t bit = bbCyclesPerBit;
  const uint32_t ps = xt_rsil(15);
  const uint32_t start = cpuCycles();
  GPOC = bbTxMask;
  uint32_t elapsed = bit;
  for (uint8_t i = 0; i < 8; i++) {
    while ((uint32_t)(cpuCycles() - start) < elapsed) {
    }
    if (value & 1) {
      GPOS = bbTxMask;
    } else {
      GPOC = bbTxMask;
    }
    value >>= 1;
    elapsed += bit;
  }
  while ((uint32_t)(cpuCycles() - start) < elapsed) {
  }
  GPOS = bbTxMask;
  elapsed += bit;
  while ((uint32_t)(cpuCycles() - start) < elapsed) {
  }
  xt_wsr_ps(ps);
}

static int bbRead() {
  const uint32_t ps = xt_rsil(15);
  if (bbHead == bbTail) {
    xt_wsr_ps(ps);
    return -1;
  }
  const uint8_t value = bbQ[bbTail];
  bbTail = (uint8_t)((bbTail + 1) & 63);
  xt_wsr_ps(ps);
  return value;
}

static bool portAvailable() {
  if (bitbang) {
    return bbHead != bbTail;
  }
  return invSerial.available() > 0;
}

static int portRead() {
  if (bitbang) {
    return bbRead();
  }
  if (!invSerial.available()) {
    return -1;
  }
  return invSerial.read();
}

static void portWrite(uint8_t value) {
  if (bitbang) {
    bbWrite(value);
  } else {
    invSerial.write(value);
  }
}

static void portWriteStr(const char *s) {
  while (s && *s) {
    portWrite(static_cast<uint8_t>(*s++));
  }
}

static void portFlushRx() {
  while (portAvailable()) {
    portRead();
  }
}

bool Inverter::begin() {
  if (bitbang) {
    bbEnd();
    bitbang = false;
  }
  if (softOn) {
    invSerial.end();
    softOn = false;
  }

  const uint32_t baud = settings.inv_baud ? settings.inv_baud : (uint32_t)INV_BAUD;
  if (baud <= kBitbangMaxBaud) {
    bbBegin(baud, settings.inv_rx, settings.inv_tx);
    bitbang = true;
  } else {
    invSerial.begin(baud, SWSERIAL_8N1, static_cast<int8_t>(settings.inv_rx),
                    static_cast<int8_t>(settings.inv_tx), false, 256);
    softOn = true;
  }
  delay(20);
  portFlushRx();
  return true;
}

uint16_t Inverter::crc16(const uint8_t *data, size_t len) const {
  uint16_t crc = 0;
  for (size_t n = 0; n < len; n++) {
    crc ^= (uint16_t)data[n] << 8;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x8000) {
        crc = (uint16_t)((crc << 1) ^ 0x1021);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

uint8_t Inverter::escapeCrc(uint8_t b) const {
  switch (b) {
    case 0x28:
      return 0x29;
    case 0x0D:
      return 0x0E;
    case 0x0A:
      return 0x0B;
    default:
      return b;
  }
}

void Inverter::keepLastRaw(const char *raw, size_t n) {
  if (n > sizeof(lastRaw)) {
    n = sizeof(lastRaw);
  }
  memcpy(lastRaw, raw, n);
  lastRawLen = static_cast<uint16_t>(n);
}

void Inverter::sendCommand(const char *cmd) {
  portFlushRx();

  const uint16_t crc = crc16(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  portWriteStr(cmd);
  portWrite(escapeCrc((crc >> 8) & 0xFF));
  portWrite(escapeCrc(crc & 0xFF));
  portWrite('\r');
}

bool Inverter::readResponse(char *raw, size_t rawLen, uint32_t timeoutMs) {
  if (!raw || rawLen < 2) {
    return false;
  }

  size_t n = 0;
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    yield();
    if (!portAvailable()) {
      delay(1);
      continue;
    }
    const int c = portRead();
    if (c < 0) {
      continue;
    }
    rxTotal++;
    if (n + 1 < rawLen) {
      raw[n++] = static_cast<char>(c);
    }
    if (c == '\r') {
      raw[n] = '\0';
      keepLastRaw(raw, n);
      return n > 1;
    }
  }
  raw[n] = '\0';
  keepLastRaw(raw, n);
  if (n > 0) {
    Serial.printf("INV timeout, %u bytes\n", (unsigned)n);
  }
  return false;
}

void Inverter::decodeResponse(char *resp) {
  if (!resp || resp[0] == '\0') {
    return;
  }

  size_t len = strlen(resp);
  while (len > 0 && (resp[len - 1] == '\r' || resp[len - 1] == '\n')) {
    resp[--len] = '\0';
  }
  if (len >= 3) {
    resp[len - 2] = '\0';
    len -= 2;
  }
  if (len > 0 && resp[0] == '(') {
    memmove(resp, resp + 1, len);
  }
}

bool Inverter::parseQpigs(const char *decoded, Telemetry &t) {
  char buf[160];
  strlcpy(buf, decoded, sizeof(buf));

  char *parts[24];
  int n = 0;
  char *save = nullptr;
  for (char *tok = strtok_r(buf, " \t", &save); tok && n < 24; tok = strtok_r(nullptr, " \t", &save)) {
    parts[n++] = tok;
  }
  if (n < 20) {
    return false;
  }

  t.grid_voltage_v = atof(parts[0]);
  t.grid_frequency_hz = atof(parts[1]);
  t.output_voltage_v = atof(parts[2]);
  t.output_frequency_hz = atof(parts[3]);
  t.output_apparent_power_va = atoi(parts[4]);
  t.output_active_power_w = atoi(parts[5]);
  t.output_load_percent = atoi(parts[6]);
  t.bus_voltage_v = atoi(parts[7]);
  t.battery_voltage_v = atof(parts[8]);
  t.battery_charge_current_a = atoi(parts[9]);
  t.battery_capacity_pct = atoi(parts[10]);
  t.inverter_heatsink_temp_c = atoi(parts[11]);
  t.pv_charge_current_a = atoi(parts[12]);
  t.pv_voltage_v = atof(parts[13]);
  t.scc_battery_voltage_v = atof(parts[14]);
  t.battery_discharge_current_a = atoi(parts[15]);
  strlcpy(t.status_bits, parts[16], sizeof(t.status_bits));
  t.pv_power_w = (n > 19) ? atoi(parts[19]) : 0;
  t.valid = true;
  t.ts = millis() / 1000;
  t.last_ok_ms = millis();
  return true;
}

const char *Inverter::mapMode(const char *resp) {
  if (!resp || resp[0] == '\0') {
    return nullptr;
  }

  char c = 0;
  const char *p = resp;
  while (*p) {
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
      c = *p;
      break;
    }
    p++;
  }
  if (!c) {
    return nullptr;
  }
  if (c >= 'a' && c <= 'z') {
    c = static_cast<char>(c - 32);
  }

  switch (c) {
    case 'L':
      return "Line";
    case 'B':
      return "Battery";
    case 'S':
      return "Standby";
    case 'P':
      return "Power On";
    case 'F':
      return "Fault";
    case 'H':
      return "Hybrid";
    default:
      return nullptr;
  }
}

bool Inverter::queryProtocol(char *out, size_t outLen) {
  if (!out || outLen == 0) {
    return false;
  }
  out[0] = '\0';

  for (int attempt = 0; attempt <= INV_RETRY; attempt++) {
    sendCommand("QPI");
    if (!readResponse(rxBuf, sizeof(rxBuf), INV_TIMEOUT_MS)) {
      continue;
    }
    decodeResponse(rxBuf);
    if (rxBuf[0] == '\0') {
      continue;
    }
    const char *p = strstr(rxBuf, "PI");
    strlcpy(out, p ? p : rxBuf, outLen);
    char *sp = strpbrk(out, " \r\n");
    if (sp) {
      *sp = '\0';
    }
    return out[0] != '\0';
  }
  return false;
}

bool Inverter::poll(Telemetry &t) {
  bool got = false;
  for (int attempt = 0; attempt <= INV_RETRY; attempt++) {
    sendCommand("QPIGS");
    if (!readResponse(rxBuf, sizeof(rxBuf), INV_TIMEOUT_MS)) {
      continue;
    }
    decodeResponse(rxBuf);
    if (parseQpigs(rxBuf, t)) {
      got = true;
      break;
    }
  }

  if (!got) {
    t.valid = false;
    t.fail_count++;
    return false;
  }

  delay(80);
  char modeRaw[32] = {0};
  for (int attempt = 0; attempt <= INV_RETRY; attempt++) {
    sendCommand("QMOD");
    if (!readResponse(rxBuf, sizeof(rxBuf), INV_TIMEOUT_MS)) {
      continue;
    }
    decodeResponse(rxBuf);
    strlcpy(modeRaw, rxBuf, sizeof(modeRaw));
    break;
  }

  const char *mode = mapMode(modeRaw);
  if (mode) {
    strlcpy(t.mode, mode, sizeof(t.mode));
  } else {
    t.mode[0] = '\0';
  }

  delay(80);
  queryPv2(t);

  t.fail_count = 0;
  return true;
}

// Второй MPPT есть не у всех моделей; без него инвертор отвечает (NAK.
void Inverter::queryPv2(Telemetry &t) {
  t.pv2_state = PV2_NO_REPLY;
  sendCommand("QPIGS2");
  if (!readResponse(rxBuf, sizeof(rxBuf), INV_TIMEOUT_MS)) {
    return;
  }
  decodeResponse(rxBuf);
  if (strstr(rxBuf, "NAK")) {
    t.pv2_state = PV2_NAK;
    return;
  }

  char buf[48];
  strlcpy(buf, rxBuf, sizeof(buf));
  char *parts[3];
  int n = 0;
  char *save = nullptr;
  for (char *tok = strtok_r(buf, " \t", &save); tok && n < 3; tok = strtok_r(nullptr, " \t", &save)) {
    parts[n++] = tok;
  }
  if (n < 3) {
    return;
  }
  t.pv2_current_a = atof(parts[0]);
  t.pv2_voltage_v = atof(parts[1]);
  t.pv2_power_w = atoi(parts[2]);
  t.pv2_state = PV2_OK;
}
