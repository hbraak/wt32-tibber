#ifndef GLOBALS_H
#define GLOBALS_H

#include <WiFi.h>
#include <IPAddress.h>
#include <freertos/semphr.h>

// Modbus server addresses
extern IPAddress remoteCERBO;
extern IPAddress remoteSOC;
extern IPAddress remoteEVCS;

// Cerbo Unit IDs
extern const uint8_t CERBO_UNIT_ID_VAL;
extern const uint8_t CERBO_UNIT_ID_TEMP_VAL;

// Connection state
extern volatile bool socConnected;
extern volatile bool evcsConnected;
extern volatile bool cerboConnected;

// Boiler
extern int boilerMode;

// Mutexes
extern SemaphoreHandle_t modbusMutex;
extern SemaphoreHandle_t lcdMutex;

// Display state
extern volatile bool displayOn;
extern volatile unsigned long lastInteractionTime;
extern int brightnessLevel;
extern int currentTab;

// Modbus data (protected by modbusMutex)
extern uint16_t socValue;
extern uint16_t chargeMode;
extern uint16_t startStopCharging;
extern uint16_t timestampHigh;
extern uint16_t timestampLow;
extern uint16_t chargePower;
extern uint16_t chargerStatus;
extern uint16_t manualModePhase;
extern uint16_t PylontechSOC;
extern uint16_t waterTemperature;
extern uint16_t dcPvPower;
extern uint16_t acPvPower[3];
extern uint16_t batteryPower;
extern uint16_t rawgridPhase1;
extern uint16_t rawgridPhase2;
extern uint16_t rawgridPhase3;
extern float totalGridPowerKW;

// SOC threshold
extern float SOC_THRESHOLD;

// Helper to take LCD mutex with timeout
#define LCD_LOCK()   xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(1000))
#define LCD_UNLOCK() xSemaphoreGive(lcdMutex)

#endif
