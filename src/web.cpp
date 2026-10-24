#include "web.h"
#include "settings.h"
#include "config.h"
#include "mqtt_ha.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

ESP8266WebServer server(80);
Telemetry telemetry = {};

// Оба варианта строки лежат во флеше; sendContent() читает их через sendContent_P().
#define TR(en, ru) (settings.lang == LANG_RU ? PSTR(ru) : PSTR(en))

static void sendEscaped(const char *s) {
  if (!s) {
    return;
  }
  for (; *s; s++) {
    switch (*s) {
      case '&':
        server.sendContent(F("&amp;"));
        break;
      case '<':
        server.sendContent(F("&lt;"));
        break;
      case '>':
        server.sendContent(F("&gt;"));
        break;
      case '"':
        server.sendContent(F("&quot;"));
        break;
      case '\'':
        server.sendContent(F("&#39;"));
        break;
      default: {
        char c[2] = {*s, 0};
        server.sendContent(c);
        break;
      }
    }
  }
}

static void sendHeader(PGM_P title) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent(settings.lang == LANG_RU ? F("<!DOCTYPE html><html lang=ru><head>")
                                              : F("<!DOCTYPE html><html lang=en><head>"));
  server.sendContent(F(
      "<meta charset=utf-8>"
      "<meta name=viewport content='width=device-width,initial-scale=1'>"
      "<title>"));
  server.sendContent(title);
  server.sendContent(F(
      "</title><style>"
      "body{font-family:Arial,Helvetica,sans-serif;margin:0;background:#111;color:#eaeaea}"
      "header{background:#036;padding:12px 16px;font-size:20px}"
      "nav a{color:#9cf;margin-right:12px;text-decoration:none}"
      "main{padding:16px;max-width:640px}"
      "label{display:block;margin:10px 0 4px}"
      "input,select{width:100%;box-sizing:border-box;padding:8px;border:1px solid #444;"
      "background:#222;color:#eee;border-radius:4px}"
      "button,.btn{display:inline-block;margin:12px 8px 0 0;padding:10px 16px;background:#1fa3ec;"
      "color:#fff;border:0;border-radius:4px;text-decoration:none;cursor:pointer}"
      ".btn-warn{background:#d43535}"
      "table{width:100%;border-collapse:collapse}"
      "td{padding:6px 0;border-bottom:1px solid #333}"
      "td:first-child{color:#aaa;width:55%}"
      ".ok{color:#6c6}.bad{color:#e66}"
      "footer{padding:16px;max-width:640px;color:#777;font-size:13px;border-top:1px solid #333}"
      "</style></head><body><header>Voltronic MQTT</header><nav style='padding:8px 16px'>"
      "<a href=/>"));
  server.sendContent(TR("Status", "Статус"));
  server.sendContent(F("</a><a href=/config>"));
  server.sendContent(TR("Settings", "Настройки"));
  server.sendContent(F("</a></nav><main>"));
}

static void sendFooter() {
  server.sendContent(F("</main></body></html>"));
  server.sendContent("");
}

static void sendRow(PGM_P name, const char *val) {
  server.sendContent(F("<tr><td>"));
  server.sendContent(name);
  server.sendContent(F("</td><td>"));
  server.sendContent(val);
  server.sendContent(F("</td></tr>"));
}

static void handleRoot() {
  sendHeader(TR("Status", "Статус"));
  server.sendContent(F("<h2>"));
  server.sendContent(TR("Connection", "Связь"));
  server.sendContent(F("</h2><table>"));

  server.sendContent(F("<tr><td>Wi-Fi</td><td class=ok>"));
  sendEscaped(WiFi.SSID().c_str());
  server.sendContent(F(" / "));
  server.sendContent(WiFi.localIP().toString());
  char rssi[24];
  snprintf(rssi, sizeof(rssi), " (%d dBm)", WiFi.RSSI());
  server.sendContent(rssi);
  server.sendContent(F("</td></tr><tr><td>MQTT</td><td class="));
  server.sendContent(mqttConnected() ? "ok>online" : "bad>offline");
  server.sendContent(F("</td></tr><tr><td>"));
  server.sendContent(TR("Broker", "Брокер"));
  server.sendContent(F("</td><td>"));
  if (settings.mqtt_host[0]) {
    sendEscaped(settings.mqtt_host);
  } else {
    server.sendContent(TR("not set", "не задан"));
  }
  server.sendContent(F("</td></tr><tr><td>"));
  server.sendContent(TR("Topic", "Топик"));
  server.sendContent(F("</td><td>"));
  sendEscaped(settings.mqtt_topic);
  server.sendContent(F("</td></tr><tr><td>"));
  server.sendContent(TR("Interval", "Интервал"));
  server.sendContent(F("</td><td>"));
  char poll[16];
  snprintf_P(poll, sizeof(poll), TR("%u s", "%u с"), settings.poll_interval);
  server.sendContent(poll);
  server.sendContent(F("</td></tr></table><h2>"));
  server.sendContent(TR("Inverter", "Инвертор"));
  server.sendContent(F("</h2><table>"));

  if (!telemetry.valid) {
    sendRow(TR("Polling", "Опрос"), TR("<span class=bad>no data</span>", "<span class=bad>нет данных</span>"));
  } else {
    char line[80];
    sendRow(TR("Mode", "Режим"), telemetry.mode[0] ? telemetry.mode : "-");
    sendRow(TR("Protocol", "Протокол"), telemetry.protocol[0] ? telemetry.protocol : "-");
    snprintf(line, sizeof(line), "%.1f V / %.1f Hz", telemetry.grid_voltage_v, telemetry.grid_frequency_hz);
    sendRow(TR("Grid", "Сеть"), line);
    snprintf(line, sizeof(line), "%.1f V / %.1f Hz", telemetry.output_voltage_v, telemetry.output_frequency_hz);
    sendRow(TR("Output", "Выход"), line);
    snprintf(line, sizeof(line), "%d W (%d%%)", telemetry.output_active_power_w, telemetry.output_load_percent);
    sendRow(TR("Load", "Нагрузка"), line);
    snprintf(line, sizeof(line), "%.2f V / %d%%", telemetry.battery_voltage_v, telemetry.battery_capacity_pct);
    sendRow(TR("Battery", "АКБ"), line);
    snprintf(line, sizeof(line), "%.1f V / %d W / %d A", telemetry.pv_voltage_v, telemetry.pv_power_w,
             telemetry.pv_charge_current_a);
    sendRow(PSTR("PV"), line);
    if (telemetry.pv2_state == PV2_OK) {
      snprintf(line, sizeof(line), "%.1f V / %d W / %.1f A", telemetry.pv2_voltage_v, telemetry.pv2_power_w,
               telemetry.pv2_current_a);
      sendRow(PSTR("PV2"), line);
    } else if (telemetry.pv2_state == PV2_NAK) {
      sendRow(PSTR("PV2"), TR("not supported (NAK)", "не поддерживается (NAK)"));
    } else {
      sendRow(PSTR("PV2"), TR("no reply to QPIGS2", "нет ответа на QPIGS2"));
    }
    snprintf(line, sizeof(line), "%d C", telemetry.inverter_heatsink_temp_c);
    sendRow(TR("Temperature", "Температура"), line);
  }
  char rxLine[32];
  snprintf(rxLine, sizeof(rxLine), "%lu", (unsigned long)inverter.rxTotal);
  sendRow(TR("Bytes received", "Принято байт"), rxLine);
  server.sendContent(F("<tr><td>"));
  server.sendContent(TR("Last reply", "Последний ответ"));
  server.sendContent(F("</td><td style='font-family:monospace;word-break:break-all'>"));
  if (inverter.lastRawLen == 0) {
    server.sendContent(TR("empty", "пусто"));
  } else {
    for (uint16_t i = 0; i < inverter.lastRawLen; i++) {
      const uint8_t b = static_cast<uint8_t>(inverter.lastRaw[i]);
      if (b >= 0x20 && b < 0x7F && b != '<' && b != '>' && b != '&') {
        char c[2] = {static_cast<char>(b), 0};
        server.sendContent(c);
      } else {
        char hex[8];
        snprintf(hex, sizeof(hex), "[%02X]", b);
        server.sendContent(hex);
      }
    }
  }
  server.sendContent(F("</td></tr>"));
  server.sendContent(F("</table><h2>RS232</h2><table>"));
  char pinLine[64];
  snprintf_P(pinLine, sizeof(pinLine), TR("%s, from converter TXD", "%s, от TXD конвертера"),
             invPinLabel(settings.inv_rx));
  sendRow(TR("Board RX", "RX платы"), pinLine);
  snprintf_P(pinLine, sizeof(pinLine), TR("%s, to converter RXD", "%s, на RXD конвертера"),
             invPinLabel(settings.inv_tx));
  sendRow(TR("Board TX", "TX платы"), pinLine);
  char baudLine[24];
  snprintf(baudLine, sizeof(baudLine), "%lu 8N1", (unsigned long)settings.inv_baud);
  sendRow(TR("Baud rate", "Скорость"), baudLine);
  sendRow(PSTR("GND / 3V3"), TR("converter GND / VCC (3.3 V only)", "GND / VCC конвертера (только 3.3V)"));
  server.sendContent(F("</table><p><a class=btn href=/config>"));
  server.sendContent(TR("MQTT / polling settings", "Настройки MQTT / опрос"));
  server.sendContent(F("</a><a class='btn btn-warn' href=/portal>"));
  server.sendContent(TR("Wi-Fi portal", "Wi-Fi портал"));
  server.sendContent(F("</a><a class=btn href=/restart>"));
  server.sendContent(TR("Restart", "Перезагрузка"));
  server.sendContent(F("</a></p></main><footer>"));
  server.sendContent(TR("Version", "Версия"));
  server.sendContent(F(" " FW_VERSION "<br>Developed by eXUnity LAB &copy; 2026</footer></body></html>"));
  server.sendContent("");
}

static void selectPin(const char *name, PGM_P label, uint8_t current) {
  server.sendContent(F("<label>"));
  server.sendContent(label);
  server.sendContent(F("</label><select name="));
  server.sendContent(name);
  server.sendContent(F(">"));
  size_t n = 0;
  const InvPinOpt *pins = invPinOptions(n);
  for (size_t i = 0; i < n; i++) {
    char opt[72];
    snprintf(opt, sizeof(opt), "<option value=%u%s>%s</option>", pins[i].gpio,
             pins[i].gpio == current ? " selected" : "", pins[i].label);
    server.sendContent(opt);
  }
  server.sendContent(F("</select>"));
}

static void selectBaud(uint32_t current) {
  server.sendContent(F("<label>"));
  server.sendContent(TR("RS232 baud rate", "Скорость RS232"));
  server.sendContent(F("</label><select name=inv_baud>"));
  size_t n = 0;
  const uint32_t *bauds = invBaudOptions(n);
  for (size_t i = 0; i < n; i++) {
    char opt[48];
    snprintf(opt, sizeof(opt), "<option value=%lu%s>%lu</option>", (unsigned long)bauds[i],
             bauds[i] == current ? " selected" : "", (unsigned long)bauds[i]);
    server.sendContent(opt);
  }
  server.sendContent(F("</select>"));
}

static void selectLang() {
  server.sendContent(F("<label>"));
  server.sendContent(TR("Interface language", "Язык интерфейса"));
  server.sendContent(F("</label><select name=lang><option value=0"));
  if (settings.lang != LANG_RU) {
    server.sendContent(F(" selected"));
  }
  server.sendContent(F(">English</option><option value=1"));
  if (settings.lang == LANG_RU) {
    server.sendContent(F(" selected"));
  }
  server.sendContent(F(">Русский</option></select>"));
}

static void field(const char *name, PGM_P label, const char *value, const char *type = "text") {
  server.sendContent(F("<label>"));
  server.sendContent(label);
  server.sendContent(F("</label><input name="));
  server.sendContent(name);
  server.sendContent(F(" type="));
  server.sendContent(type);
  server.sendContent(F(" value=\""));
  sendEscaped(value);
  server.sendContent(F("\">"));
}

static void handleConfig() {
  sendHeader(TR("Settings", "Настройки"));
  server.sendContent(F("<form method=POST action=/save>"));
  selectLang();
  field("mqtt_host", TR("MQTT broker", "MQTT брокер"), settings.mqtt_host);
  char port[8];
  snprintf(port, sizeof(port), "%u", settings.mqtt_port);
  field("mqtt_port", TR("MQTT port", "MQTT порт"), port, "number");
  field("mqtt_user", TR("MQTT user", "MQTT логин"), settings.mqtt_user);
  field("mqtt_pass", TR("MQTT password", "MQTT пароль"), settings.mqtt_pass, "password");
  field("mqtt_topic", TR("JSON topic", "Топик JSON"), settings.mqtt_topic);
  field("mqtt_status", TR("Status topic", "Топик статуса"), settings.mqtt_status);
  field("device_name", TR("Device name", "Имя устройства"), settings.device_name);
  field("device_id", TR("Home Assistant ID", "ID для Home Assistant"), settings.device_id);
  char poll[8];
  snprintf(poll, sizeof(poll), "%u", settings.poll_interval);
  field("poll_interval", TR("Poll interval, s (2–120)", "Интервал опроса, сек (2–120)"), poll, "number");
  selectPin("inv_rx", TR("Board RX (to converter TXD)", "RX платы (подключить к TXD конвертера)"), settings.inv_rx);
  selectPin("inv_tx", TR("Board TX (to converter RXD)", "TX платы (подключить к RXD конвертера)"), settings.inv_tx);
  selectBaud(settings.inv_baud);
  server.sendContent(F("<label><input type=checkbox name=ha_discovery value=1"));
  if (settings.ha_discovery) {
    server.sendContent(F(" checked"));
  }
  server.sendContent(F("> "));
  server.sendContent(TR("MQTT Discovery for Home Assistant", "MQTT Discovery для Home Assistant"));
  server.sendContent(F("</label><p><button type=submit>"));
  server.sendContent(TR("Save", "Сохранить"));
  server.sendContent(F("</button></p></form>"));
  sendFooter();
}

static void copyArg(const char *name, char *dst, size_t dstLen) {
  if (server.hasArg(name)) {
    strlcpy(dst, server.arg(name).c_str(), dstLen);
  }
}

static void handleSave() {
  copyArg("mqtt_host", settings.mqtt_host, sizeof(settings.mqtt_host));
  if (server.hasArg("mqtt_port")) {
    settings.mqtt_port = (uint16_t)server.arg("mqtt_port").toInt();
  }
  copyArg("mqtt_user", settings.mqtt_user, sizeof(settings.mqtt_user));
  copyArg("mqtt_pass", settings.mqtt_pass, sizeof(settings.mqtt_pass));
  copyArg("mqtt_topic", settings.mqtt_topic, sizeof(settings.mqtt_topic));
  copyArg("mqtt_status", settings.mqtt_status, sizeof(settings.mqtt_status));
  copyArg("device_name", settings.device_name, sizeof(settings.device_name));
  copyArg("device_id", settings.device_id, sizeof(settings.device_id));
  if (server.hasArg("poll_interval")) {
    settings.poll_interval = (uint16_t)server.arg("poll_interval").toInt();
  }
  if (server.hasArg("inv_rx")) {
    settings.inv_rx = (uint8_t)server.arg("inv_rx").toInt();
  }
  if (server.hasArg("inv_tx")) {
    settings.inv_tx = (uint8_t)server.arg("inv_tx").toInt();
  }
  if (server.hasArg("inv_baud")) {
    settings.inv_baud = (uint32_t)server.arg("inv_baud").toInt();
  }
  if (server.hasArg("lang")) {
    settings.lang = (uint8_t)server.arg("lang").toInt();
  }
  settings.ha_discovery = server.hasArg("ha_discovery");
  settingsSave();
  inverter.begin();
  mqttForceReconnect();

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

static void sendMessagePage(PGM_P text, bool refresh) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent(F("<!DOCTYPE html><html><head><meta charset=utf-8>"));
  if (refresh) {
    server.sendContent(F("<meta http-equiv=refresh content='3;url=/'>"));
  }
  server.sendContent(F("</head><body style='background:#111;color:#eee;font-family:Arial;padding:20px'>"));
  server.sendContent(text);
  server.sendContent(F("</body></html>"));
  server.sendContent("");
}

static void handleRestart() {
  sendMessagePage(TR("Restarting...", "Перезагрузка..."), true);
  delay(200);
  ESP.restart();
}

void webStop() {
  server.stop();
}

static bool webHandlersBound = false;

void webSetup() {
  if (!webHandlersBound) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/restart", HTTP_GET, handleRestart);
    server.on("/portal", HTTP_GET, []() {
      sendMessagePage(TR("Opening the Wi-Fi portal. Connect to the device access point.",
                         "Открываю портал Wi-Fi. Подключитесь к точке доступа устройства."),
                      false);
      delay(200);
      startWifiPortal(true);
    });
    webHandlersBound = true;
  }
  server.begin();
}

void webLoop() {
  server.handleClient();
}

static void applyPortalParams(WiFiManagerParameter &host, WiFiManagerParameter &port, WiFiManagerParameter &user,
                              WiFiManagerParameter &pass, WiFiManagerParameter &topic, WiFiManagerParameter &poll) {
  strlcpy(settings.mqtt_host, host.getValue(), sizeof(settings.mqtt_host));
  settings.mqtt_port = (uint16_t)atoi(port.getValue());
  strlcpy(settings.mqtt_user, user.getValue(), sizeof(settings.mqtt_user));
  strlcpy(settings.mqtt_pass, pass.getValue(), sizeof(settings.mqtt_pass));
  strlcpy(settings.mqtt_topic, topic.getValue(), sizeof(settings.mqtt_topic));
  settings.poll_interval = (uint16_t)atoi(poll.getValue());
  settingsSave();
}

void startWifiPortal(bool force) {
  webStop();
  WiFi.mode(WIFI_STA);

  char portBuf[8];
  char pollBuf[8];
  snprintf(portBuf, sizeof(portBuf), "%u", settings.mqtt_port);
  snprintf(pollBuf, sizeof(pollBuf), "%u", settings.poll_interval);

  WiFiManager wm;
  wm.setTitle("Voltronic MQTT");
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(20);
  wm.setBreakAfterConfig(true);

  WiFiManagerParameter p_host("mqtt_host", "MQTT host", settings.mqtt_host, sizeof(settings.mqtt_host) - 1);
  WiFiManagerParameter p_port("mqtt_port", "MQTT port", portBuf, sizeof(portBuf) - 1);
  WiFiManagerParameter p_user("mqtt_user", "MQTT user", settings.mqtt_user, sizeof(settings.mqtt_user) - 1);
  WiFiManagerParameter p_pass("mqtt_pass", "MQTT password", settings.mqtt_pass, sizeof(settings.mqtt_pass) - 1);
  WiFiManagerParameter p_topic("mqtt_topic", "MQTT topic", settings.mqtt_topic, sizeof(settings.mqtt_topic) - 1);
  WiFiManagerParameter p_poll("poll", "Poll interval, sec", pollBuf, sizeof(pollBuf) - 1);

  wm.addParameter(&p_host);
  wm.addParameter(&p_port);
  wm.addParameter(&p_user);
  wm.addParameter(&p_pass);
  wm.addParameter(&p_topic);
  wm.addParameter(&p_poll);

  bool shouldSave = false;
  wm.setSaveParamsCallback([&shouldSave]() { shouldSave = true; });
  wm.setSaveConfigCallback([&shouldSave]() { shouldSave = true; });

  bool ok;
  if (force) {
    ok = wm.startConfigPortal(settingsApName());
  } else {
    ok = wm.autoConnect(settingsApName());
  }

  if (shouldSave) {
    applyPortalParams(p_host, p_port, p_user, p_pass, p_topic, p_poll);
  }

  WiFi.mode(WIFI_STA);
  mqttForceReconnect();
  if (ok) {
    Serial.printf("WiFi %s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F("WiFi portal timeout"));
  }
  webSetup();
}

bool wifiConnectOrPortal(bool forcePortal) {
  WiFi.hostname(settingsClientId());
  WiFi.mode(WIFI_STA);
  if (forcePortal) {
    startWifiPortal(true);
    return WiFi.status() == WL_CONNECTED;
  }
  startWifiPortal(false);
  return WiFi.status() == WL_CONNECTED;
}
