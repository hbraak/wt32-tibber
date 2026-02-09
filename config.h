#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// WiFi Configuration
// ============================================================
#define PRIMARY_SSID     "FRITZ!Box Fon WLAN 7390"
#define PRIMARY_PASSWORD "4551516174768576"

#define SECONDARY_SSID     "SLC"
#define SECONDARY_PASSWORD "82603690157953239701"

// Static IP Configuration
#define STATIC_IP      192, 168, 178, 155
#define GATEWAY_IP     192, 168, 178, 1
#define SUBNET_MASK    255, 255, 255, 0
#define PRIMARY_DNS    8, 8, 8, 8
#define SECONDARY_DNS  8, 8, 4, 4

// ============================================================
// Tibber API
// ============================================================
#define TIBBER_API_URL   "https://api.tibber.com/v1-beta/gql"
#define TIBBER_API_TOKEN "ha0Aetr1iaF9oY1ZsxF3eEBZYPYWpq7rjv0NLA0mmu8"

// ============================================================
// Modbus Servers
// ============================================================
#define SOC_SERVER_IP    192, 168, 178, 121
#define EVCS_SERVER_IP   192, 168, 178, 78
#define CERBO_SERVER_IP  192, 168, 178, 65

#define CERBO_UNIT_ID      100
#define CERBO_UNIT_ID_TEMP 24

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
#define MODBUS_TASK_STACK    16384  // Increased from 8192
#define MODBUS_WRITE_STACK   8192
#define TOUCH_TASK_STACK     8192
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
