#ifndef MODBUS_HELPERS_H
#define MODBUS_HELPERS_H

#include <IPAddress.h>

bool writeModbusData(IPAddress server, int reg, uint16_t value, uint8_t unitID);
void readModbusData(IPAddress server, int reg, uint16_t &value, uint8_t unitID);
#endif
