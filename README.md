# esp8266-voltronic-mqtt

**Developed by eXUnity LAB © 2026**

ESP8266 (NodeMCU v3) firmware that polls a Voltronic / Axpert hybrid inverter over RS232 and publishes the data to MQTT with Home Assistant auto-discovery.

The device replaces a PC with a USB-RS232 cable: it plugs into the inverter's RJ45 port and sends the data over Wi-Fi.

[Русский](README.ru.md) | **English**

## Features

- Polls `QPIGS` (grid, output, load, battery, PV1, temperature), `QMOD` (operating mode) and `QPI` (protocol version).
- Second PV input via `QPIGS2` on models with two MPPTs. PV2 sensors appear only if the inverter answers this command.
- MQTT: all data as JSON in one topic, `online` / `offline` availability via Last Will.
- Home Assistant MQTT Discovery: sensors are created automatically.
- Web UI in English or Russian (English by default): status page and settings for MQTT, poll interval, RS232 pins and baud rate.
- First Wi-Fi setup through an access point, like Tasmota.
- Diagnostics on the status page: number of bytes received from RS232 and the last raw inverter reply.

Tested on a Voltronic Axpert Ultra with an RS232 port on an RJ45 connector.

## What you need

| Part | Notes |
|---|---|
| NodeMCU v3 (ESP-12E, ESP8266) | Any ESP8266 board with GPIO4/GPIO5 broken out will do |
| MAX3232 RS232-TTL module with a DB9 connector | Must work from 3.3 V. MAX232 modules (without the "3") are designed for 5 V |
| RJ45 (8P8C) cable | The stock inverter RJ45–DB9 cable or a home-made one |
| 5 V supply for the NodeMCU | USB charger or a DC-DC converter from the battery |

## Wiring

```text
Inverter RJ45  ──RS232 ±V──  MAX3232 (DB9)  ──TTL 3.3 V──  NodeMCU
```

### TTL side: MAX3232 module and NodeMCU

| NodeMCU | MAX3232 module |
|---|---|
| D1 (GPIO5), receive | TXD |
| D2 (GPIO4), transmit | RXD |
| 3V3 | VCC |
| GND | GND |

> [!WARNING]
> Power the module from 3V3 only. With 5 V on the module, its TXD pin outputs 5 V, which can damage the ESP8266.

TXD/RXD labels mean different things on modules from different vendors. If there is no data, swap the wires on D1 and D2, or reassign the pins in the settings; no soldering needed.

Pins and baud rate are changed in the web UI. Do not use D8 (GPIO15) for receive: the ESP8266 won't boot if this pin is held high.

### RS232 side: inverter RJ45

RJ45 pinout on Voltronic inverters (pins counted left to right, looking at the plug with the latch facing down):

| RJ45 | Signal |
|---|---|
| 1 | Inverter TX (data out) |
| 2 | Inverter RX (data in) |
| 8 | GND |

The output of one side always goes to the input of the other. Which DB9 pin on the module is the output and which is the input depends on the module's connector.

#### Option 1: male DB9 on the module (pins)

The module is wired like a PC COM port: pin 2 is the input, pin 3 is the output. The stock inverter RJ45–DB9 cable plugs straight into the module.

| Inverter RJ45 | Male DB9 on the module |
|---|---|
| 1 (inverter TX) | 2 (module RX) |
| 2 (inverter RX) | 3 (module TX) |
| 8 (GND) | 5 (GND) |

#### Option 2: female DB9 on the module (sockets)

The module is meant to plug into a PC and is wired the other way round: pin 2 is the output, pin 3 is the input. Lines 2 and 3 must be crossed.

| Inverter RJ45 | Female DB9 on the module |
|---|---|
| 1 (inverter TX) | 3 (module RX) |
| 2 (inverter RX) | 2 (module TX) |
| 8 (GND) | 5 (GND) |

If you use the stock inverter cable with a female module, put a male–male null-modem adapter (pins 2 and 3 crossed) between them. A plain straight-through gender changer won't work: the module's output ends up on the inverter's output and there is no link.

#### Checking with a voltmeter

An idle RS232 output holds a negative voltage. Power the module without connecting it to the inverter and measure pins 2 and 3 against pin 5 (GND):

- about −5…−6 V is the module's output (TX); connect it to RJ45 pin 2;
- about 0 V is the module's input (RX); connect it to RJ45 pin 1.

On the inverter, RJ45 pin 1 against pin 8 should idle at about −5…−12 V.

## Flashing

You need [PlatformIO](https://platformio.org/) (the VS Code / Cursor extension or the CLI).

```text
git clone https://github.com/ibuben/esp8266-voltronic-mqtt.git
cd esp8266-voltronic-mqtt
pio run -t upload
```

With several COM ports connected, pick the right one: `pio run -t upload --upload-port COM8`.

Serial log: `pio device monitor` (115200 baud).

## First setup

1. After flashing, the board starts an open access point named `voltronic-XXXX`.
2. Connect to it from your phone and the portal opens. If it doesn't open by itself, go to `192.168.4.1`.
3. Choose your Wi-Fi network, enter the MQTT broker address, user, password, topic and poll interval.
4. After saving, the board joins your Wi-Fi. Its IP address is shown in your router or in the serial log.

To reopen the Wi-Fi portal later, hold the FLASH button for 3 seconds or press "Wi-Fi portal" in the web UI.

## Web UI

Open the board's IP address in a browser. The UI is in English by default; switch to Russian in "Settings → Interface language".

**Status**: Wi-Fi, MQTT, current inverter data, PV2, received byte counter and the last inverter reply, current RS232 pins and baud rate.

**Settings**:

| Field | Default |
|---|---|
| Interface language | English (Russian available) |
| MQTT broker, port, user, password | —, 1883 |
| JSON topic | `home/axpert-ultra/tele` |
| Status topic | `home/axpert-ultra/status` |
| Device name / Home Assistant ID | `Axpert Ultra` / `axpert-ultra` |
| Poll interval | 5 s (2–120) |
| Board RX / TX | D1 / D2 |
| RS232 baud rate | 2400 (Voltronic always uses 2400) |
| MQTT Discovery for Home Assistant | on |

Settings are stored in flash. New pins and baud rate take effect right after saving, without a reboot.

### LED

| LED | Meaning |
|---|---|
| Solid on | MQTT connected, inverter data is coming in |
| Slow blink | Wi-Fi is up, but no MQTT or no inverter data |
| Fast blink | No Wi-Fi |

## MQTT

Data is published to the JSON topic on every poll cycle:

```json
{
  "grid_voltage_v": 230.1,
  "grid_frequency_hz": 49.9,
  "output_voltage_v": 230.0,
  "output_frequency_hz": 49.9,
  "output_apparent_power_va": 460,
  "output_active_power_w": 410,
  "output_load_percent": 7,
  "bus_voltage_v": 390,
  "battery_voltage_v": 52.4,
  "battery_charge_current_a": 0,
  "battery_capacity_pct": 85,
  "inverter_heatsink_temp_c": 35,
  "pv_charge_current_a": 0,
  "pv_voltage_v": 0.0,
  "scc_battery_voltage_v": 0.0,
  "battery_discharge_current_a": 8,
  "status_bits": "00010000",
  "pv_power_w": 0,
  "pv2_current_a": 0.0,
  "pv2_voltage_v": 0.0,
  "pv2_power_w": 0,
  "pv_total_power_w": 0,
  "protocol": "PI30",
  "mode": "Battery",
  "ts": 1234
}
```

The values above are only an example. The `pv2_*` and `pv_total_power_w` fields are present only on inverters that answer `QPIGS2`.

The status topic gets `online` on connect and `offline` via Last Will when the board drops off the network.

Home Assistant sensors are published to `homeassistant/sensor/<ID>_<sensor>/config` and grouped into one device.

## No data?

Check the "Bytes received" and "Last reply" rows on the status page.

| What you see | Where to look |
|---|---|
| 0 bytes | Nothing reaches the board: DB9 pins 2 and 3 are swapped (see Wiring), TXD/RXD are swapped on the TTL side, or there is no common GND |
| Bytes, but garbage | Wrong baud rate, or the module is not powered from 3.3 V |
| A readable reply like `(230.0 50.0 …` | The link works. Open an issue and attach this reply |

Testing the module without the inverter: short pins 2 and 3 on its DB9. "Last reply" should show the board's own `QPIGS` command followed by two CRC bytes and `[0D]`.

Bench testing with a PC: cheap CH340-based USB-RS232 adapters may not work correctly at 2400 baud, so the terminal shows garbage even though the board transmits correctly. Use a built-in COM port or an FTDI / CP2102 adapter for such tests.

## License

Copyright © 2026 eXUnity LAB. Released under the [MIT](LICENSE) license.
