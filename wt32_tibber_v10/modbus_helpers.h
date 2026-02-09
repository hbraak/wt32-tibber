#ifndef MODBUS_HELPERS_H
#define MODBUS_HELPERS_H

#include <IPAddress.h>
#include <ModbusTCP.h>

// Must be called with modbusMutex held
bool writeModbusData(IPAddress server, int reg, uint16_t value, uint8_t unitID);
void readModbusData(IPAddress server, int reg, uint16_t &value, uint8_t unitID = 1);
bool connectModbusServer(IPAddress server, int maxRetries = 2);

extern ModbusTCP mb;

#endif
