# WT32-SC01 Plus Energy Dashboard

ESP32-S3 basiertes Energie-Dashboard für das WT32-SC01 Plus Display (ST7796, 480×320).

## Features

### Tab 1 — Übersicht
- **Wallbox**: SOC, Ladeleistung, Lademodus (Zeitplan/Sofort/Auto), Start/Stop, 1P/3P
- **PV**: Aktuelle Solarleistung (DC + AC)
- **Batterie**: Pylontech SOC + Lade-/Entladeleistung
- **Netz**: Aktuelle Netzleistung + Tages-Bezug (VRM API)
- **Wetter**: Aktuelles Wetter mit Icon (OpenWeatherMap)
- **Boiler**: Wassertemperatur
- **VRM Tageswerte**: PV-Ertrag | Eigenverbrauch% | Einspeisung (kWh)

### Tab 2 — Uhr & Boiler
- Uhrzeit, Boiler-Steuerung (Auto/2kW/4kW/6kW)

### Tab 3 — Tibber Preisgraph
- 48h Strompreis-Balkendiagramm (heute + morgen)
- Aktueller Preis hervorgehoben

### Tab 4 — Wettervorhersage
- 5-Tage Temperaturkurve
- Niederschlagsbars mit mm-Skala
- Tägliche Wetter-Icons

## Datenquellen

| Quelle | Protokoll | Daten |
|--------|-----------|-------|
| Cerbo GX | Modbus TCP | PV, Batterie, Netz, Wassertemp |
| EVCS (Wallbox) | Modbus TCP | Lademodus, Leistung, Status |
| SOC-Server | Modbus TCP | Auto-SOC, Timestamp |
| Tibber API | HTTPS/GraphQL | Strompreise 48h |
| VRM API | HTTPS/REST | Tageswerte (Solar, Verbrauch, Netz) |
| OpenWeatherMap | HTTP/REST | Aktuell + 5-Tage Forecast |

## Hardware

- **Display**: WT32-SC01 Plus (ESP32-S3, ST7796, kapazitiver Touch)
- **FQBN**: `esp32:esp32:esp32s3:CDCOnBoot=cdc`
- **OTA**: Port 3232, Hostname `WT32-OTA-Display`
- **Statische IP**: 192.168.178.155

## Setup

1. `credentials.h` erstellen (siehe `credentials.h.example`):
   - WiFi SSID/Passwort
   - Tibber API Token
   - VRM Login (E-Mail/Passwort)
   - OpenWeatherMap API Key
2. Arduino IDE oder `arduino-cli` mit ESP32-S3 Board
3. Benötigte Libraries: LovyanGFX, ModbusTCP, ArduinoJson, ArduinoOTA

## Architektur

- **FreeRTOS Tasks**: modbusTask (Core 0), modbusWriteTask (Core 0), touchTask (Core 1)
- **Mutexe**: lcdMutex (Display), modbusMutex (Modbus-Client)
- **Watchdog**: 60s Timeout, alle Tasks registriert

## Bekannte Einschränkungen

- Flash-Nutzung bei 93% — wenig Platz für weitere Features
- VRM Token läuft nach ~24h ab (wird automatisch erneuert)
- Forecast-Daten: 16KB DynamicJsonDocument auf Heap
