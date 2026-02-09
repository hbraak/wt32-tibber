# WT32 Tibber Energy Display

ESP32-based energy dashboard for home solar/battery systems with Tibber electricity pricing.

![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Display](https://img.shields.io/badge/display-WT32--SC01%20Plus-green)

## What it does

A touchscreen energy monitor that shows:
- **Tibber electricity prices** — current price + 48h bar chart (today + tomorrow)
- **EV charging control** — Start/stop, charge mode, SOC, phase switching (via go-eCharger Modbus)
- **Solar PV production** — DC + AC coupled power from Victron Cerbo GX
- **Battery SOC** — Pylontech battery state of charge + charge/discharge power
- **Grid power** — 3-phase grid import/export
- **Boiler control** — 2kW/4kW/6kW manual override via relay switching
- **Water temperature** — from Cerbo GX sensor
- **Auto-charging cutoff** — stops EV charging when SOC exceeds threshold (80/90/100%)

## Hardware

- **WT32-SC01 Plus** (ESP32-S3 + 3.5" 480×320 capacitive touch, ST7796 driver)
- **Victron Cerbo GX** — Modbus TCP for battery, PV, grid, temperature, relays
- **go-eCharger** — Modbus TCP for EV charging control
- **Custom SOC server** — Modbus TCP for EV battery SOC

## Features

- 3-tab UI: Overview | Clock/Boiler | Price Graph
- Touch controls with debouncing
- Auto display off after 2 min inactivity
- Auto brightness (day/night)
- OTA firmware updates
- Dual WiFi fallback
- Watchdog timer (60s)
- Thread-safe LCD + Modbus access via FreeRTOS mutexes

## Architecture

```
Core 0: modbusTask (reads all Modbus devices every 20s)
        modbusWriteTask (processes write queue)
Core 1: touchTask (touch input handling)
        loop() (WiFi check, Tibber API, OTA, display timeout)
```

## Setup

### Dependencies (Arduino IDE / PlatformIO)

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [ModbusTCP](https://github.com/emelianov/modbus-esp8266) (modbus-esp8266)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (v6)
- [TimeLib](https://github.com/PaulStoffregen/Time)
- Arduino ESP32 core (ESP32-S3)

### Configuration

Edit `config.h`:
- WiFi credentials (primary + secondary)
- Static IP address
- Tibber API token (get yours at [developer.tibber.com](https://developer.tibber.com))
- Modbus server IPs for your devices

### Upload

1. Connect WT32-SC01 Plus via USB
2. Select board: **ESP32S3 Dev Module**
3. Upload via Arduino IDE or PlatformIO
4. Subsequent updates via OTA: `WT32-OTA-Display.local`

## Branches

- **`main`** — Stable version with fixes (watchdog, mutexes, memory management)
- **`original`** — Original code before refactoring

See [ANALYSIS.md](ANALYSIS.md) for detailed stability analysis.

## License

MIT
