#include "boiler.h"
#include "globals.h"
#include "modbus_helpers.h"

static const int RELAY1_REG = 806;
static const int RELAY2_REG = 807;

void setBoilerPower(int powerLevel) {
    uint16_t r1 = 0, r2 = 0;
    switch (powerLevel) {
        case 2: r1 = 1; r2 = 0; break;
        case 4: r1 = 0; r2 = 1; break;
        case 6: r1 = 1; r2 = 1; break;
        default: r1 = 0; r2 = 0; break;  // 0 = off/auto
    }
    
    xSemaphoreTake(modbusMutex, portMAX_DELAY);
    writeModbusData(remoteCERBO, RELAY1_REG, r1, CERBO_UNIT_ID_VAL);
    writeModbusData(remoteCERBO, RELAY2_REG, r2, CERBO_UNIT_ID_VAL);
    xSemaphoreGive(modbusMutex);
}

void syncBoilerStatus() {
    uint16_t relay1Status = 0, relay2Status = 0;

    xSemaphoreTake(modbusMutex, portMAX_DELAY);
    readModbusData(remoteCERBO, RELAY1_REG, relay1Status, CERBO_UNIT_ID_VAL);
    readModbusData(remoteCERBO, RELAY2_REG, relay2Status, CERBO_UNIT_ID_VAL);
    xSemaphoreGive(modbusMutex);

    if (relay1Status == 0 && relay2Status == 1) boilerMode = 2;
    else if (relay1Status == 1 && relay2Status == 0) boilerMode = 4;
    else if (relay1Status == 1 && relay2Status == 1) boilerMode = 6;
    else boilerMode = 0;
}

void toggleBoilerMode() {
    // Fixed: cycle through valid modes 0 -> 2 -> 4 -> 6 -> 0
    static const int modes[] = {0, 2, 4, 6};
    static const int numModes = 4;
    
    int currentIndex = 0;
    for (int i = 0; i < numModes; i++) {
        if (modes[i] == boilerMode) { currentIndex = i; break; }
    }
    boilerMode = modes[(currentIndex + 1) % numModes];
    
    // Non-blocking: just set the power, don't delay
    setBoilerPower(boilerMode);
    
    Serial.printf("Boiler mode changed to: %d\n", boilerMode);
}
