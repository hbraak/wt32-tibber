#include "globals.h"
#include "modbus_helpers.h"
#include "boiler.h"  // Stelle sicher, dass die Boiler-Header-Datei eingebunden ist
#include <TFT_eSPI.h>
extern TFT_eSPI lcd;  // Verweise auf das in der Hauptdatei deklarierte LCD-Objekt

#define ERROR_X 50
#define ERROR_Y 100
#define ERROR_WIDTH 200
#define ERROR_HEIGHT 50
#define TFT_RED 0xF800
#define TFT_WHITE 0xFFFF

// Boilersteuerung
void setBoilerPower(int powerLevel) {
    const int RELAY1_REG = 806;
    const int RELAY2_REG = 807;

    switch (powerLevel) {
        case 0:
            writeModbusData(remoteCERBO, RELAY1_REG, 0, CERBO_UNIT_ID);
            writeModbusData(remoteCERBO, RELAY2_REG, 0, CERBO_UNIT_ID);
            break;
        case 2:
            writeModbusData(remoteCERBO, RELAY1_REG, 1, CERBO_UNIT_ID);
            writeModbusData(remoteCERBO, RELAY2_REG, 0, CERBO_UNIT_ID);
            break;
        case 4:
            writeModbusData(remoteCERBO, RELAY1_REG, 0, CERBO_UNIT_ID);
            writeModbusData(remoteCERBO, RELAY2_REG, 1, CERBO_UNIT_ID);
            break;
        case 6:
            writeModbusData(remoteCERBO, RELAY1_REG, 1, CERBO_UNIT_ID);
            writeModbusData(remoteCERBO, RELAY2_REG, 1, CERBO_UNIT_ID);
            break;
        default:
            Serial.println("Ungültiger Leistungswert für den Boiler.");
            break;
    }
}


void syncBoilerStatus() {
    uint16_t relay1Status = 0;
    uint16_t relay2Status = 0;
    const int RELAY1_REG = 806;
    const int RELAY2_REG = 807;

    // Status der Relais auslesen
    readModbusData(remoteCERBO, RELAY1_REG, relay1Status, CERBO_UNIT_ID);
    readModbusData(remoteCERBO, RELAY2_REG, relay2Status, CERBO_UNIT_ID);

    // Modus basierend auf dem Status der Relais festlegen
    if (relay1Status == 0 && relay2Status == 1) {
        boilerMode = 2;  // 2kW-Modus
    } else if (relay1Status == 1 && relay2Status == 0) {
        boilerMode = 4;  // 4kW-Modus
    } else if (relay1Status == 1 && relay2Status == 1) {
        boilerMode = 6;  // 6kW-Modus
    } else {
        boilerMode = 0;  // Auto-Modus oder aus
    }
}

void toggleBoilerMode() {
    // Zyklischer Wechsel durch die Modi: Auto (0), 2kW (2), 4kW (4), 6kW (6)
    boilerMode = (boilerMode + 1) % 4;  // Die Modi sind: 0 (Auto), 2 (2kW), 4 (4kW), 6 (6kW)

    // BoilerSwitch visuell sofort aktualisieren
    drawBoilerSwitch();
    // Verzögerung von 1 Sekunde (1000 ms) einfügen, bevor das Relais über Modbus gesteuert wird
    delay(1000);

    // Boiler entsprechend dem neuen Modus steuern
    switch (boilerMode) {
        case 0:
            setBoilerPower(0);  // Auto-Modus
            break;
        case 2:
            setBoilerPower(2);  // 2kW-Modus
            break;
        case 4:
            setBoilerPower(4);  // 4kW-Modus
            break;
        case 6:
            setBoilerPower(6);  // 6kW-Modus
            break;
    }

    // Nach weiteren 2 Sekunden den Status überprüfen, um sicherzustellen, dass das Relais richtig geschaltet wurde
    delay(2000);
    syncBoilerStatus();  // Relaisstatus auslesen und Boiler-Switch erneut aktualisieren

    // Wenn der Modus nicht korrekt umgeschaltet wurde, eine Fehlermeldung anzeigen
    if (boilerMode != getCurrentBoilerMode()) {
        displayError("Fehler beim Umschalten des Boiler-Modus.");
    }
    
    // BoilerSwitch erneut zeichnen (nach der Statusüberprüfung)
    drawBoilerSwitch();
}


void displayError(const char* message) {
    lcd.fillRect(ERROR_X, ERROR_Y, ERROR_WIDTH, ERROR_HEIGHT, TFT_RED); // Fehleranzeige-Bereich
    lcd.setTextColor(TFT_WHITE);
    lcd.setCursor(ERROR_X, ERROR_Y);
    lcd.print(message);
}

int getCurrentBoilerMode() {
    // Status der Relais erneut abfragen
    syncBoilerStatus();
    return boilerMode;
}

