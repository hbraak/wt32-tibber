#ifndef GLOBALS_H
#define GLOBALS_H

#include <WiFi.h>  // Falls IPAddress benötigt wird
#include <IPAddress.h>  // Für die IP-Adresse des CERBO

// Deklariere die Variablen als extern, damit sie in mehreren Dateien verwendet werden können
extern IPAddress remoteCERBO;
extern int boilerMode;
extern const uint8_t CERBO_UNIT_ID;
#endif
