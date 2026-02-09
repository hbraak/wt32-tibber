#include "modbus_helpers.h"
#include "globals.h"
#include <esp_task_wdt.h>

ModbusTCP mb;

bool connectModbusServer(IPAddress server, int maxRetries) {
    for (int i = 0; i < maxRetries; i++) {
        esp_task_wdt_reset();  // Reset WDT during retries
        Serial.printf("Connecting to Modbus %s (attempt %d/%d)\n", 
                       server.toString().c_str(), i + 1, maxRetries);
        if (mb.connect(server)) {
            Serial.println("Connected.");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // Reduced from 2000ms
    }
    Serial.printf("Failed to connect to %s after %d attempts\n", 
                   server.toString().c_str(), maxRetries);
    return false;
}

void readModbusData(IPAddress server, int reg, uint16_t &value, uint8_t unitID) {
    if (!mb.isConnected(server)) {
        if (!mb.connect(server)) {
            Serial.printf("Read: Cannot connect to %s\n", server.toString().c_str());
            return;
        }
    }

    uint16_t trans = mb.readHreg(server, reg, &value, 1, NULL, unitID);
    uint32_t startMillis = millis();
    
    while (mb.isTransaction(trans)) {
        mb.task();
        vTaskDelay(pdMS_TO_TICKS(10));
        if (millis() - startMillis > 4000) {
            Serial.printf("Read timeout: reg %d on %s\n", reg, server.toString().c_str());
            break;
        }
    }
}

bool writeModbusData(IPAddress server, int reg, uint16_t value, uint8_t unitID) {
    if (!mb.isConnected(server)) {
        if (!mb.connect(server)) {
            Serial.printf("Write: Cannot connect to %s\n", server.toString().c_str());
            return false;
        }
    }

    uint16_t trans = mb.writeHreg(server, reg, value, NULL, unitID);
    uint32_t startMillis = millis();

    while (mb.isTransaction(trans)) {
        mb.task();
        vTaskDelay(pdMS_TO_TICKS(10));
        if (millis() - startMillis > 5000) {
            Serial.printf("Write timeout: reg %d on %s\n", reg, server.toString().c_str());
            return false;
        }
    }

    Serial.printf("Write OK: reg %d = %d on %s\n", reg, value, server.toString().c_str());
    return true;
}
