#ifndef CONFIG_H
#define CONFIG_H

// Credentials in separate file (not in git!)
#include "credentials.h"

// ============================================================
// WiFi - Static IP Configuration
// ============================================================
#define STATIC_IP      192, 168, 178, 155
#define GATEWAY_IP     192, 168, 178, 1
#define SUBNET_MASK    255, 255, 255, 0
#define PRIMARY_DNS    8, 8, 8, 8
#define SECONDARY_DNS  8, 8, 4, 4

// ============================================================
// Tibber API
// ============================================================
#define TIBBER_API_URL   "https://api.tibber.com/v1-beta/gql"

// ============================================================
// Modbus Servers
// ============================================================
#define SOC_SERVER_IP    192, 168, 178, 121
#define EVCS_SERVER_IP   192, 168, 178, 78
#define CERBO_SERVER_IP  192, 168, 178, 65

#define CERBO_UNIT_ID      100
#define CERBO_UNIT_ID_TEMP 24

// ============================================================
// VRM API (Victron Remote Management)
// ============================================================
#define VRM_SITE_ID    136727
#define VRM_UPDATE_MS  300000  // 5 minutes

// ============================================================
// OpenWeatherMap API
// ============================================================
#define WEATHER_CITY_ID    "2886242"  // Köln
#define WEATHER_UPDATE_MS  1800000    // 30 minutes

// ============================================================
// Display
// ============================================================
#define TFT_WIDTH       320
#define TFT_HEIGHT      480
#define TFT_GREY        0x5AEB
#define TFT_OFF_DELAY   120000  // 2 minutes
#define BRIGHTNESS_DAY  255
#define BRIGHTNESS_NIGHT 10

// ============================================================
// Task Configuration
// ============================================================
#define MODBUS_TASK_STACK    16384
#define MODBUS_WRITE_STACK   8192
#define TOUCH_TASK_STACK     12288
#define MODBUS_LONG_INTERVAL  20000
#define MODBUS_SHORT_INTERVAL 2000

// ============================================================
// Watchdog
// ============================================================
#define WDT_TIMEOUT_SEC  60

// ============================================================
// WiFi Check Interval
// ============================================================
#define WIFI_CHECK_INTERVAL 30000  // 30 seconds

#endif // CONFIG_H
