# WT32 Tibber Display – Code-Analyse

## Architektur-Überblick

ESP32-basiertes Energie-Dashboard (WT32-SC01 Plus mit ST7796 Display) das:
- **Tibber API** abfragt für Strompreise (GraphQL über HTTPS)
- **Modbus TCP** kommuniziert mit: EVCS (Wallbox), SOC-Server (E-Auto), Cerbo GX (Victron)
- **3-Tab UI** mit Touch: Übersicht, Uhr/Boiler, Preisgraph
- **OTA Updates** unterstützt
- **FreeRTOS Tasks**: modbusTask (Core 0), modbusWriteTask (Core 0), touchTask (Core 1)

## Kritische Stabilitätsprobleme

### 1. 🔴 Stack Overflow in modbusTask / fetchTibberPrices
`StaticJsonDocument<8192>` wird auf dem Stack allokiert, aber der Task hat nur `TASK_STACK_SIZE = 8192` Bytes. Das JSON-Dokument allein füllt den gesamten Stack → **garantierter Stack Overflow**.

### 2. 🔴 Race Condition auf LCD
- `modbusTask` ruft `displayData()` auf (Core 0)
- `touchTask` ruft `switchTab()`, `drawXxx()` auf (Core 1)
- Kein Mutex für LCD-Zugriff → **Display-Korruption, Crashes**

### 3. 🔴 Race Condition auf ModbusTCP-Objekt
Das `mb`-Objekt wird von modbusTask, modbusWriteTask UND checkWiFiConnection() (loop/Core 1) benutzt. Der Mutex wird nicht in allen Pfaden korrekt gehalten.

### 4. 🔴 Boiler-Modus Cycling Bug
```cpp
boilerMode = (boilerMode + 1) % 4;  // Ergibt 0,1,2,3
```
Aber gültige Modi sind 0, 2, 4, 6. Modus 1 und 3 sind ungültig → **Boiler-Fehlfunktion**.

### 5. 🟠 Kein Watchdog Timer
Wenn ein Task hängt (z.B. Modbus-Timeout, WiFi-Reconnect), gibt es keinen Watchdog der das System neu startet.

### 6. 🟠 WiFi Reconnect blockiert
`connectToWiFi()` blockiert bis zu 20 Sekunden (40 × 500ms delay). Wird aus `checkWiFiConnection()` im loop() aufgerufen → **blockiert OTA und alle loop()-Logik**.

### 7. 🟠 Blocking delays in Touch-Handler
`toggleBoilerMode()` hat `delay(1000)` + `delay(2000)` = 3 Sekunden Blocking im touchTask → **UI friert ein**.

### 8. 🟠 fetchTibberPrices() wird aus loop() aufgerufen (nicht thread-safe)
`tibberPrices[]` wird aus loop() geschrieben und aus modbusTask gelesen (für displayData → drawTibberPriceGraph).

### 9. 🟡 Hardcoded Credentials
- WiFi-Passwörter im Quellcode
- Tibber API-Token im Quellcode
- Statische IP hardcoded

### 10. 🟡 Fehlende Fehlerbehandlung bei Tibber API
- Kein HTTPS-Zertifikat-Check
- Kein Retry bei fehlgeschlagener API-Abfrage
- HTTPClient wird nicht bei allen Fehlerpfaden geschlossen

### 11. 🟡 turnOnDisplay() ruft lcd.init() auf
Volle Display-Reinitialisierung nur um Backlight einzuschalten – unnötig und langsam.

### 12. 🟡 Array Out-of-Bounds Risk
`drawTibberPriceGraph` iteriert `for (int i = 1; i < 24; i++)` für min/max, aber zeichnet `for (int i = 0; i < size; i++)` mit size=48. Wenn Preise 0.0 sind (nicht geladen), wird die Skalierung falsch.

### 13. 🟡 Modbus connect/disconnect pro Lesevorgang
`readModbusData()` verbindet und trennt bei JEDEM Register-Read. Bei ~15 Reads pro Zyklus = 30 TCP-Handshakes alle 20 Sekunden. Extrem ineffizient.

### 14. 🟡 checkWiFiConnection() dupliziert Modbus-Reads
Nach WiFi-Reconnect werden alle Modbus-Daten nochmal gelesen – das macht der modbusTask sowieso. Doppelte Arbeit + Race Condition.

## Zusammenfassung

| Kategorie | Anzahl |
|-----------|--------|
| Kritisch (Crash/Corruption) | 4 |
| Wichtig (Stabilität) | 5 |
| Minor (Code Quality) | 5 |

**Hauptursachen für Instabilität**: Stack Overflow beim JSON-Parsing, LCD Race Conditions, fehlender Watchdog.
