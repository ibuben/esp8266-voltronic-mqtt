# esp8266-voltronic-mqtt

Прошивка для ESP8266 (NodeMCU v3), которая опрашивает гибридный инвертор Voltronic / Axpert по RS232 и публикует данные в MQTT с автообнаружением в Home Assistant.

Устройство заменяет компьютер с USB-RS232-кабелем: подключается к порту RJ45 инвертора и отправляет данные по Wi-Fi.

[English summary below](#english)

## Возможности

- Опрос `QPIGS` (сеть, выход, нагрузка, АКБ, PV1, температура), `QMOD` (режим работы), `QPI` (версия протокола).
- Второй вход PV через `QPIGS2` у моделей с двумя MPPT. Датчики PV2 появляются, только если инвертор отвечает на эту команду.
- MQTT: JSON с данными в один топик, статус `online` / `offline` через Last Will.
- MQTT Discovery для Home Assistant: датчики создаются автоматически.
- Веб-интерфейс на английском или русском (английский по умолчанию): страница статуса и настройки MQTT, интервала опроса, пинов и скорости RS232.
- Первая настройка Wi-Fi через точку доступа, как у Tasmota.
- Диагностика на странице статуса: число принятых с RS232 байт и последний сырой ответ инвертора.

Проверено на Voltronic Axpert Ultra с портом RS232 на разъёме RJ45.

## Что понадобится

| Деталь | Примечание |
|---|---|
| NodeMCU v3 (ESP-12E, ESP8266) | Подойдёт любая плата на ESP8266 с выведенными GPIO4/GPIO5 |
| Модуль RS232-TTL на MAX3232 с разъёмом DB9 | Должен работать от 3,3 В. Модули на MAX232 (без «3») рассчитаны на 5 В |
| Кабель с RJ45 (8P8C) | Родной кабель инвертора RJ45–DB9 или самодельный |
| Питание 5 В для NodeMCU | USB-зарядка или DC-DC от АКБ |

## Схема подключения

```text
Инвертор RJ45  ──RS232 ±V──  MAX3232 (DB9)  ──TTL 3,3 В──  NodeMCU
```

### Сторона TTL: модуль MAX3232 и NodeMCU

| NodeMCU | Модуль MAX3232 |
|---|---|
| D1 (GPIO5), приём | TXD |
| D2 (GPIO4), передача | RXD |
| 3V3 | VCC |
| GND | GND |

> [!WARNING]
> Питайте модуль только от 3V3. Если подать на модуль 5 В, на выводе TXD будет 5 В, а это может повредить ESP8266.

Подписи TXD/RXD на модулях разных производителей означают разное. Если данных нет, поменяйте местами провода на D1 и D2 или переназначьте пины в настройках, перепаивать не нужно.

Пины и скорость меняются в веб-интерфейсе. Для приёма не используйте D8 (GPIO15): при высоком уровне на этом пине ESP8266 не загружается.

### Сторона RS232: RJ45 инвертора

Распиновка RJ45 у инверторов Voltronic (контакты считаются слева направо, если смотреть на защёлку снизу):

| RJ45 | Сигнал |
|---|---|
| 1 | TX инвертора (выход данных) |
| 2 | RX инвертора (вход данных) |
| 8 | GND |

Выход одной стороны всегда соединяется со входом другой. Какой вывод DB9 у модуля выход, а какой вход, зависит от разъёма модуля.

#### Вариант 1: на модуле разъём «папа» (штырьки)

Модуль устроен как COM-порт компьютера: вывод 2 — вход, вывод 3 — выход. Родной кабель инвертора RJ45–DB9 вставляется прямо в модуль.

| RJ45 инвертора | DB9 модуля «папа» |
|---|---|
| 1 (TX инвертора) | 2 (RX модуля) |
| 2 (RX инвертора) | 3 (TX модуля) |
| 8 (GND) | 5 (GND) |

#### Вариант 2: на модуле разъём «мама» (гнёзда)

Модуль рассчитан на подключение к компьютеру и устроен наоборот: вывод 2 — выход, вывод 3 — вход. Линии 2 и 3 нужно перекрестить.

| RJ45 инвертора | DB9 модуля «мама» |
|---|---|
| 1 (TX инвертора) | 3 (RX модуля) |
| 2 (RX инвертора) | 2 (TX модуля) |
| 8 (GND) | 5 (GND) |

Если используете родной кабель инвертора, а у модуля «мама», поставьте между ними нуль-модемный переходник «папа–папа», в котором 2 и 3 перекрещены. Обычный прямой переходник не подойдёт: выход модуля попадёт на выход инвертора, и связи не будет.

#### Как проверить вольтметром

Выход RS232 в покое держит отрицательное напряжение. Включите модуль, не подключая его к инвертору, и измерьте напряжение на выводах 2 и 3 относительно вывода 5 (GND):

- около −5…−6 В — это выход модуля (TX), его соединяют с RJ45[2];
- около 0 В — это вход модуля (RX), его соединяют с RJ45[1].

На инверторе: на RJ45[1] относительно RJ45[8] в покое должно быть около −5…−12 В.

## Прошивка

Нужен [PlatformIO](https://platformio.org/) (расширение для VS Code или Cursor, либо CLI).

```text
git clone https://github.com/ibuben/esp8266-voltronic-mqtt.git
cd esp8266-voltronic-mqtt
pio run -t upload
```

Если подключено несколько COM-портов, укажите нужный: `pio run -t upload --upload-port COM8`.

Лог работы: `pio device monitor` (115200 бод).

## Первая настройка

1. После прошивки плата поднимает точку доступа `voltronic-XXXX` без пароля.
2. Подключитесь к ней с телефона, откроется портал. Если он не открылся сам, зайдите на `192.168.4.1`.
3. Выберите свою сеть Wi-Fi, укажите адрес MQTT-брокера, логин, пароль, топик и интервал опроса.
4. После сохранения плата подключится к Wi-Fi. Её адрес виден в роутере или в логе порта.

Повторно открыть портал Wi-Fi: удерживайте кнопку FLASH 3 секунды или нажмите «Wi-Fi портал» в веб-интерфейсе.

## Веб-интерфейс

Откройте адрес платы в браузере. Интерфейс по умолчанию на английском; русский выбирается в «Settings → Interface language».

**Статус**: Wi-Fi, MQTT, текущие данные инвертора, PV2, счётчик принятых байт и последний ответ инвертора, текущие пины и скорость RS232.

**Настройки**:

| Поле | По умолчанию |
|---|---|
| Язык интерфейса | English (переключается на «Русский») |
| MQTT брокер, порт, логин, пароль | —, 1883 |
| Топик JSON | `home/axpert-ultra/tele` |
| Топик статуса | `home/axpert-ultra/status` |
| Имя устройства / ID для Home Assistant | `Axpert Ultra` / `axpert-ultra` |
| Интервал опроса | 5 с (2–120) |
| RX / TX платы | D1 / D2 |
| Скорость RS232 | 2400 (у Voltronic всегда 2400) |
| MQTT Discovery для Home Assistant | включено |

Настройки сохраняются во флеш-памяти. Новые пины и скорость применяются сразу после сохранения, без перезагрузки.

### Светодиод

| Светодиод | Состояние |
|---|---|
| Горит | MQTT подключён, данные от инвертора идут |
| Мигает медленно | Wi-Fi есть, но нет MQTT или данных от инвертора |
| Мигает быстро | Нет Wi-Fi |

## MQTT

Данные публикуются в топик JSON на каждом цикле опроса:

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

Значения в примере условные. Поля `pv2_*` и `pv_total_power_w` есть только у инверторов, которые отвечают на `QPIGS2`.

В топик статуса пишется `online` при подключении и `offline` через Last Will, если плата пропала из сети.

Датчики Home Assistant публикуются в `homeassistant/sensor/<ID>_<датчик>/config` и собираются в одно устройство.

## Если данных нет

На странице статуса посмотрите строки «Принято байт» и «Последний ответ» (в английском интерфейсе — «Bytes received» и «Last reply»).

| Что видно | Где искать |
|---|---|
| 0 байт | До платы ничего не доходит: перепутаны выводы 2 и 3 на DB9 (см. схему подключения), перепутаны TXD/RXD на стороне TTL, нет общего GND |
| Байты есть, но мусор | Не та скорость или модуль питается не от 3,3 В |
| Читаемый ответ вида `(230.0 50.0 …` | Связь есть. Откройте issue и приложите этот ответ |

Проверка модуля без инвертора: замкните выводы 2 и 3 на его DB9. В «Последний ответ» должна прийти собственная команда платы `QPIGS` с двумя байтами CRC и `[0D]`.

Проверка на столе через компьютер: дешёвые USB-RS232-переходники на CH340 могут неправильно работать на 2400 бод, и тогда терминал показывает мусор, хотя плата передаёт верно. Для такой проверки используйте встроенный COM-порт или переходник на FTDI / CP2102.

## Лицензия

[MIT](LICENSE)

---

<a id="english"></a>

## English

ESP8266 (NodeMCU v3) firmware that polls a Voltronic / Axpert hybrid inverter over RS232 and publishes the data to MQTT with Home Assistant auto-discovery. It replaces a PC with a USB-RS232 cable on the inverter's RJ45 port. Tested on a Voltronic Axpert Ultra.

**Features:** `QPIGS`, `QMOD` and `QPI` polling; second PV input via `QPIGS2` (sensors appear only if the inverter answers it); JSON state topic plus an `online` / `offline` availability topic; English / Russian web UI (English by default) for MQTT, poll interval, RS232 pins and baud rate; Tasmota-style Wi-Fi setup portal; raw RS232 diagnostics on the status page.

**Parts:** NodeMCU v3, a 3.3 V MAX3232 RS232-TTL module with a DB9 connector, an RJ45 cable.

**TTL wiring:** D1 (GPIO5) ← module TXD, D2 (GPIO4) → module RXD, 3V3 → VCC, GND → GND. Power the module from 3.3 V only. Pins can be changed in the web UI. Do not use D8 (GPIO15) for RX, because the board won't boot if it is held high.

**Inverter RJ45:** pin 1 = inverter TX, pin 2 = inverter RX, pin 8 = GND.

| RJ45 | Male DB9 module | Female DB9 module |
|---|---|---|
| 1 (inverter TX) | 2 | 3 |
| 2 (inverter RX) | 3 | 2 |
| 8 (GND) | 5 | 5 |

A male DB9 module is wired like a PC port, so the stock inverter RJ45–DB9 cable plugs straight in. A female DB9 module has pins 2 and 3 swapped, so cross them or use a male–male null-modem adapter. To check with a voltmeter: the module's TX output idles at about −5…−6 V relative to pin 5.

**Build and flash:** `pio run -t upload` (PlatformIO). On first boot, connect to the `voltronic-XXXX` access point and enter your Wi-Fi and MQTT settings. Hold FLASH for 3 s to reopen the portal.

**Troubleshooting:** if "Bytes received" on the status page stays at 0, the DB9 pins 2/3 or the TTL TXD/RXD lines are swapped, or GND is missing. Some CH340 USB-RS232 adapters fail at 2400 baud during bench tests, so use a built-in COM port or an FTDI / CP2102 adapter.

License: [MIT](LICENSE).
