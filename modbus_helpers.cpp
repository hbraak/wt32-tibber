#include "modbus_helpers.h"
#include "ModbusTCP.h"  // Modbus Library

extern ModbusTCP mb;  // Globale Deklaration des Modbus-Objekts

// Liest Modbus-Daten sequentiell von jedem Server
void readModbusData(IPAddress server, int reg, uint16_t &value, uint8_t unitID) {
  Serial.printf("Reading Modbus data from %s, Register: %d, Unit ID: %d\n", server.toString().c_str(), reg, unitID);

  // Verbindung herstellen
  if (!mb.connect(server)) {
    Serial.printf("Failed to connect to Modbus server: %s\n", server.toString().c_str());
    return;
  }
  
  uint16_t trans = mb.readHreg(server, reg, &value, 1, NULL, unitID);
  uint32_t startMillis = millis();
  while (mb.isTransaction(trans)) {
    mb.task();
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    if (millis() - startMillis > 4000) { 
      Serial.println("Modbus transaction timeout.");
      break;
    }
  }

  // Überprüfung, ob die Transaktion erfolgreich war
  if (!mb.isTransaction(trans)) {
    Serial.printf("Read success: Register %d = %d\n", reg, value);
  } else {
    Serial.println("Read failed or timed out.");
  }

  // Verbindung trennen
  mb.disconnect(server);
  Serial.printf("Disconnected from Modbus server: %s\n", server.toString().c_str());
}

bool writeModbusData(IPAddress server, int reg, uint16_t value, uint8_t unitID) {
  Serial.printf("Writing Modbus data to %s, Register: %d, Value: %d, Unit ID: %d\n", server.toString().c_str(), reg, value, unitID);

  // Verbindung herstellen
  if (!mb.connect(server)) {
    Serial.printf("Failed to connect to Modbus server: %s\n", server.toString().c_str());
    return false;
  }
  
  uint16_t trans = mb.writeHreg(server, reg, value, NULL, unitID);
  uint32_t startMillis = millis();
  
  while (mb.isTransaction(trans)) {
    mb.task();
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    if (millis() - startMillis > 4000) {
      Serial.println("Modbus transaction timeout. Attempting to reconnect...");

      mb.disconnect(server);
      delay(1000);  // Eine Sekunde warten, bevor neu verbunden wird
      if (mb.connect(server)) {
        Serial.println("Reconnected to Modbus server successfully.");

        // Erneuter Schreibversuch
        trans = mb.writeHreg(server, reg, value, NULL, unitID);
        startMillis = millis();
        
        while (mb.isTransaction(trans)) {
          mb.task();
          vTaskDelay(10 / portTICK_PERIOD_MS);
          
          if (millis() - startMillis > 5000) {
            Serial.println("Modbus transaction failed after reconnect.");
            mb.disconnect(server);
            return false;  // Fehlgeschlagene Transaktion
          }
        }

        Serial.println("Modbus transaction successful after reconnect.");
        mb.disconnect(server);
        return true;
      } else {
        Serial.println("Modbus reconnection failed.");
        return false;
      }
    }
  }

  Serial.println("Modbus transaction successful.");
  mb.disconnect(server);
  return true;
}
