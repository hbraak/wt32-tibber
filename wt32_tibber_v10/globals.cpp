#include "globals.h"
#include "config.h"

IPAddress remoteCERBO(CERBO_SERVER_IP);
IPAddress remoteSOC(SOC_SERVER_IP);
IPAddress remoteEVCS(EVCS_SERVER_IP);

const uint8_t CERBO_UNIT_ID_VAL = CERBO_UNIT_ID;
const uint8_t CERBO_UNIT_ID_TEMP_VAL = CERBO_UNIT_ID_TEMP;

volatile bool socConnected = false;
volatile bool evcsConnected = false;
volatile bool cerboConnected = false;

int boilerMode = 0;

SemaphoreHandle_t modbusMutex = NULL;
SemaphoreHandle_t lcdMutex = NULL;

volatile bool displayOn = true;
volatile unsigned long lastInteractionTime = 0;
int brightnessLevel = BRIGHTNESS_DAY;
int currentTab = 1;

uint16_t socValue = 0;
uint16_t chargeMode = 0;
uint16_t startStopCharging = 0;
uint16_t timestampHigh = 0;
uint16_t timestampLow = 0;
uint16_t chargePower = 0;
uint16_t chargerStatus = 0;
uint16_t manualModePhase = 0;
uint16_t PylontechSOC = 0;
uint16_t waterTemperature = 0;
uint16_t dcPvPower = 0;
uint16_t acPvPower[3] = {0};
uint16_t batteryPower = 0;
uint16_t rawgridPhase1 = 0;
uint16_t rawgridPhase2 = 0;
uint16_t rawgridPhase3 = 0;
float totalGridPowerKW = 0.0;
float SOC_THRESHOLD = 80.0;
