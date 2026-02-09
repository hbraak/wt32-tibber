#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <ModbusTCP.h>
#include <TimeLib.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "globals.h"
#include "modbus_helpers.h"
#include "tibber.h"
#include "boiler.h"

// ============================================================
// Display Driver
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_FT5x06  _touch_instance;

public:
  LGFX(void) {
    auto cfg = _bus_instance.config();
    cfg.freq_write = 40000000;
    cfg.pin_wr = 47; cfg.pin_rd = -1; cfg.pin_rs = 0;
    cfg.pin_d0 = 9; cfg.pin_d1 = 46; cfg.pin_d2 = 3; cfg.pin_d3 = 8;
    cfg.pin_d4 = 18; cfg.pin_d5 = 17; cfg.pin_d6 = 16; cfg.pin_d7 = 15;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);

    auto cfg_panel = _panel_instance.config();
    cfg_panel.pin_cs = -1; cfg_panel.pin_rst = 4; cfg_panel.pin_busy = -1;
    cfg_panel.panel_width = TFT_WIDTH; cfg_panel.panel_height = TFT_HEIGHT;
    cfg_panel.offset_x = 0; cfg_panel.offset_y = 0; cfg_panel.offset_rotation = 0;
    cfg_panel.dummy_read_pixel = 8; cfg_panel.dummy_read_bits = 1;
    cfg_panel.readable = false; cfg_panel.invert = true;
    cfg_panel.rgb_order = false; cfg_panel.dlen_16bit = false; cfg_panel.bus_shared = false;
    _panel_instance.config(cfg_panel);

    auto cfg_light = _light_instance.config();
    cfg_light.pin_bl = 45; cfg_light.invert = false;
    cfg_light.freq = 44100; cfg_light.pwm_channel = 7;
    _light_instance.config(cfg_light);
    _panel_instance.setLight(&_light_instance);

    auto cfg_touch = _touch_instance.config();
    cfg_touch.x_min = 0; cfg_touch.x_max = 319;
    cfg_touch.y_min = 0; cfg_touch.y_max = 479;
    cfg_touch.pin_int = 7; cfg_touch.bus_shared = true;
    cfg_touch.offset_rotation = 0;
    cfg_touch.i2c_port = 1; cfg_touch.i2c_addr = 0x38;
    cfg_touch.pin_sda = 6; cfg_touch.pin_scl = 5; cfg_touch.freq = 400000;
    _touch_instance.config(cfg_touch);
    _panel_instance.setTouch(&_touch_instance);

    setPanel(&_panel_instance);
  }
};

static LGFX lcd;

// ============================================================
// Modbus Registers
// ============================================================
const int SOC_REG = 1;
const int TIMESTAMP_HIGH_REG = 2;
const int TIMESTAMP_LOW_REG = 3;
const int CHARGE_POWER_REG = 5014;
const int CHARGER_STATUS_REG = 5015;
const int MANUAL_MODE_PHASE_REG = 5055;
const int CHARGE_MODE_REG = 5009;
const int START_STOP_CHARGING_REG = 5010;
const int PYLONTECH_SOC_REG = 843;
const int DC_PV_POWER_REG = 850;
const int AC_PV_POWER_REGS[] = {811, 812, 813};
const int GRID_PHASE1_REG = 820;
const int GRID_PHASE2_REG = 821;
const int GRID_PHASE3_REG = 822;
const int WATER_TEMP_REG = 3304;

// ============================================================
// UI Layout Constants
// ============================================================
const int BORDER = 10;
const int ICON_HEIGHT = 70;
const int ICON_WIDTH0 = 70;
const int ICON_WIDTH1 = 102;
const int ICON_WIDTH2 = 150;
const int SPACE = 5;

#define BOILER_SWITCH_X BORDER
#define BOILER_SWITCH_Y BORDER
#define BOILER_SWITCH_W 120
#define BOILER_SWITCH_H 40

#define CAR_RANGE_X BORDER
#define CAR_RANGE_Y (BORDER + ICON_HEIGHT + SPACE)
#define CAR_RANGE_WIDTH ICON_WIDTH1
#define CAR_RANGE_HEIGHT ICON_HEIGHT

#define CAR_ICON_X BORDER
#define CAR_ICON_Y BORDER
#define CAR_ICON_WIDTH ICON_WIDTH1
#define CAR_ICON_HEIGHT ICON_HEIGHT

#define HOUSE_ICON_X (BORDER + ICON_WIDTH0 + ICON_WIDTH1 + 2*SPACE)
#define HOUSE_ICON_Y (BORDER + SPACE + 2*ICON_HEIGHT + SPACE)
#define HOUSE_ICON_WIDTH ICON_WIDTH0
#define HOUSE_ICON_HEIGHT ICON_HEIGHT

#define TIBBER_RECT_X (BORDER + ICON_WIDTH1 + SPACE)
#define TIBBER_RECT_Y (BORDER + SPACE + 2*ICON_HEIGHT + SPACE)
#define TIBBER_RECT_SIZE ICON_HEIGHT

#define H2O_RECT_X (BORDER + ICON_WIDTH0 + ICON_WIDTH1 + ICON_WIDTH2 + 3*SPACE)
#define H2O_RECT_Y (BORDER + SPACE + 2*ICON_HEIGHT + SPACE)
#define H2O_RECT_SIZE ICON_HEIGHT

#define SOC_RECT_X (BORDER + ICON_WIDTH0 + ICON_WIDTH1 + ICON_WIDTH2 + 3*SPACE)
#define SOC_RECT_Y BORDER
#define SOC_RECT_SIZE ICON_HEIGHT

#define START_STOP_RECT_X (BORDER + SPACE + ICON_WIDTH1)
#define START_STOP_RECT_Y BORDER
#define START_STOP_RECT_SIZE ICON_HEIGHT

#define BUTTON_X (BORDER + ICON_WIDTH0 + ICON_WIDTH1 + 2*SPACE)
#define BUTTON_Y BORDER
#define BUTTON_W ICON_WIDTH2
#define BUTTON_H ICON_HEIGHT

#define MANUAL_MODE_PHASE_RECT_X (BORDER + SPACE + ICON_WIDTH1)
#define MANUAL_MODE_PHASE_RECT_Y (BORDER + ICON_HEIGHT + SPACE)
#define MANUAL_MODE_PHASE_RECT_W ICON_HEIGHT
#define MANUAL_MODE_PHASE_RECT_H ICON_HEIGHT

#define BRIGHTNESS_RECT_X 439
#define BRIGHTNESS_RECT_Y 1
#define BRIGHTNESS_RECT_W 40
#define BRIGHTNESS_RECT_H 40

#define TAB1_BUTTON_X 439
#define TAB1_BUTTON_Y 61
#define TAB1_BUTTON_W 40
#define TAB1_BUTTON_H 40

#define TAB2_BUTTON_X 439
#define TAB2_BUTTON_Y 121
#define TAB2_BUTTON_W 40
#define TAB2_BUTTON_H 40

#define TAB3_BUTTON_X 439
#define TAB3_BUTTON_Y 181
#define TAB3_BUTTON_W 40
#define TAB3_BUTTON_H 40

#define SUN_ICON_X BORDER
#define SUN_ICON_Y (CAR_ICON_Y + SPACE + 2*ICON_HEIGHT + SPACE)
#define SUN_ICON_WIDTH 40
#define SUN_ICON_HEIGHT 40

#define GRID_ICON_X BORDER
#define GRID_ICON_Y (CAR_ICON_Y + 3*SPACE + 3*ICON_HEIGHT)
#define GRID_ICON_WIDTH ICON_WIDTH1
#define GRID_ICON_HEIGHT ICON_HEIGHT

#define WIFI_ICON_X 439
#define WIFI_ICON_Y 280
#define WIFI_ICON_SIZE 40

#include "carIcon.h"
#include "houseIcon.h"
#include "sunicon.h"

// ============================================================
// Rectangle State for SOC Threshold
// ============================================================
enum RectangleState { GREEN, YELLOW, RED };
RectangleState rectangleState = GREEN;

// ============================================================
// Modbus Write Queue
// ============================================================
struct ModbusWriteRequest {
    IPAddress server;
    int reg;
    uint16_t value;
    uint8_t unitID;
};
QueueHandle_t modbusWriteQueue;

void queueModbusWrite(IPAddress server, int reg, uint16_t value, uint8_t unitID = MODBUSIP_UNIT) {
    ModbusWriteRequest request = {server, reg, value, unitID};
    if (xQueueSend(modbusWriteQueue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("Warning: Modbus write queue full!");
    }
}

// ============================================================
// Touch debounce
// ============================================================
static unsigned long lastTouchTime = 0;
const unsigned long debounceDelay = 200;
static volatile bool immediateModbusRequest = false;

// ============================================================
// Forward declarations
// ============================================================
void displayData();
void switchTab(int tab);
void drawBoilerSwitch();
void drawClockTab();
void drawTibberPrice();
void drawTibberPriceGraph(float prices[], int size);
void adjustBrightness(int level);

// ============================================================
// Drawing Functions (all require LCD_LOCK to be held by caller)
// ============================================================

void drawWiFiIcon(bool connected) {
    uint16_t bg = connected ? TFT_GREEN : TFT_LIGHTGREY;
    lcd.fillRect(WIFI_ICON_X, WIFI_ICON_Y, WIFI_ICON_SIZE, WIFI_ICON_SIZE, bg);
    lcd.drawRect(WIFI_ICON_X, WIFI_ICON_Y, WIFI_ICON_SIZE, WIFI_ICON_SIZE, TFT_BLACK);
    int cx = WIFI_ICON_X + WIFI_ICON_SIZE / 2;
    int cy = WIFI_ICON_Y + WIFI_ICON_SIZE / 2 + 10;
    lcd.drawArc(cx, cy, WIFI_ICON_SIZE / 2, WIFI_ICON_SIZE / 2, 225, 315, TFT_BLACK);
    lcd.drawArc(cx, cy, WIFI_ICON_SIZE / 3, WIFI_ICON_SIZE / 3, 225, 315, TFT_BLACK);
    if (connected) lcd.fillCircle(cx, cy, 3, TFT_BLACK);
}

void drawBoilerSwitch() {
    lcd.fillRect(BOILER_SWITCH_X, BOILER_SWITCH_Y, BOILER_SWITCH_W, BOILER_SWITCH_H, TFT_GREY);
    lcd.drawRect(BOILER_SWITCH_X, BOILER_SWITCH_Y, BOILER_SWITCH_W, BOILER_SWITCH_H, TFT_BLACK);
    String modeText;
    switch (boilerMode) {
        case 0: modeText = "Auto"; break;
        case 2: modeText = "2kW"; break;
        case 4: modeText = "4kW"; break;
        case 6: modeText = "6kW"; break;
        default: modeText = "?"; break;
    }
    lcd.setTextSize(1);
    int tw = lcd.textWidth(modeText);
    lcd.setCursor(BOILER_SWITCH_X + (BOILER_SWITCH_W - tw) / 2,
                  BOILER_SWITCH_Y + (BOILER_SWITCH_H - lcd.fontHeight()) / 2);
    lcd.print(modeText);
}

void drawTibberPrice() {
    int rx = TIBBER_RECT_X, ry = TIBBER_RECT_Y;
    int rw = ICON_WIDTH0, rh = ICON_WIDTH0;
    lcd.fillRect(rx, ry, rw, rh, TFT_DARKGREEN);
    lcd.drawRect(rx, ry, rw, rh, TFT_BLACK);
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    String text;
    if (currentElectricityPrice > 0) {
        text = String(int(currentElectricityPrice * 100)) + " c";
    } else {
        text = "N/A";
    }
    int tw = lcd.textWidth(text);
    lcd.setCursor(rx + (rw - tw) / 2, ry + (rh - lcd.fontHeight()) / 2);
    lcd.print(text);
}

void drawTibberPriceGraph(float tibberPrices[], int size) {
    int graphX = 50, graphY = 50;
    int graphHeight = lcd.height() * 2 / 3;
    int totalGapWidth = size - 1;
    int availableWidth = lcd.width() - 130;
    int barWidth = (availableWidth - totalGapWidth) / size;
    int graphWidth = (barWidth * size) + totalGapWidth;

    lcd.fillRect(graphX, graphY, graphWidth / 2, graphHeight, TFT_WHITE);
    uint16_t palePink = lcd.color565(255, 228, 240);
    lcd.fillRect(graphX + graphWidth / 2, graphY, graphWidth / 2, graphHeight, palePink);
    lcd.drawRect(graphX, graphY, graphWidth, graphHeight, TFT_BLACK);

    // Find price range (only valid prices)
    float minPrice = 999, maxPrice = -999;
    bool hasValid = false;
    for (int i = 0; i < 24; i++) {
        if (tibberPrices[i] > 0.001) {
            float p = tibberPrices[i] * 100;
            if (p < minPrice) minPrice = p;
            if (p > maxPrice) maxPrice = p;
            hasValid = true;
        }
    }
    if (!hasValid) { minPrice = 0; maxPrice = 40; }
    maxPrice += 3; minPrice -= 3;
    float priceRange = maxPrice - minPrice;
    if (priceRange < 1) priceRange = 1;

    lcd.setTextSize(1);
    lcd.setTextColor(TFT_BLACK);
    lcd.setCursor(graphX, graphY - 25);
    lcd.print("Tibber Preis (Cent/kWh)");

    const char* germanWeekdays[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char todayLabel[24], tomorrowLabel[24];
        snprintf(todayLabel, sizeof(todayLabel), "%s, %02d.%02d",
                 germanWeekdays[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1);
        timeinfo.tm_mday += 1;
        mktime(&timeinfo);
        snprintf(tomorrowLabel, sizeof(tomorrowLabel), "%s, %02d.%02d",
                 germanWeekdays[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1);
        lcd.setCursor(graphX + 20, graphY + graphHeight + 5);
        lcd.print(todayLabel);
        lcd.setCursor(graphX + graphWidth / 2 + 20, graphY + graphHeight + 5);
        lcd.print(tomorrowLabel);
    }

    for (int i = 0; i <= 5; i++) {
        float tickPrice = minPrice + i * (priceRange / 5.0);
        int tickY = graphY + graphHeight - (i * graphHeight / 5);
        lcd.drawLine(graphX, tickY, graphX + graphWidth, tickY, TFT_LIGHTGREY);
        lcd.setCursor(graphX - 30, tickY - 5);
        lcd.printf("%.0f", tickPrice);
    }

    for (int i = 0; i < size; i += 6) {
        int tickX = graphX + (i * (barWidth + 1));
        lcd.drawLine(tickX, graphY, tickX, graphY + graphHeight, TFT_LIGHTGREY);
    }

    int currentHour = getCurrentHour();
    for (int i = 0; i < size; i++) {
        if (tibberPrices[i] < 0.001) continue;
        int x = graphX + (i * (barWidth + 1));
        int barHeight = ((tibberPrices[i] * 100 - minPrice) / priceRange) * graphHeight;
        int y = graphY + graphHeight - barHeight;
        uint16_t barColor = (i == currentHour) ? TFT_RED : TFT_BLUE;
        lcd.fillRect(x, y, barWidth, barHeight, barColor);
    }

    if (currentHour >= 0 && currentHour < size) {
        int currentX = graphX + (currentHour * (barWidth + 1));
        lcd.setTextColor(TFT_DARKGREEN);
        lcd.setCursor(currentX + 5, graphY + 5);
        lcd.printf("%dh:", currentHour);
        lcd.setCursor(currentX + 55, graphY + 5);
        lcd.printf("%d cent", int(tibberPrices[currentHour] * 100));
    }
}

void drawBrightnessButton() {
    lcd.fillRect(BRIGHTNESS_RECT_X, BRIGHTNESS_RECT_Y, BRIGHTNESS_RECT_W, BRIGHTNESS_RECT_H, TFT_GREY);
    lcd.drawRect(BRIGHTNESS_RECT_X, BRIGHTNESS_RECT_Y, BRIGHTNESS_RECT_W, BRIGHTNESS_RECT_H, TFT_BLACK);
    lcd.setTextSize(1); lcd.setTextColor(TFT_BLACK);
    int tw = lcd.textWidth("B");
    lcd.setCursor(BRIGHTNESS_RECT_X + (BRIGHTNESS_RECT_W - tw) / 2,
                  BRIGHTNESS_RECT_Y + (BRIGHTNESS_RECT_H - lcd.fontHeight()) / 2);
    lcd.print("B");
}

void drawSunIcon() {
    const int containerW = 102, containerH = 70;
    lcd.fillRect(SUN_ICON_X, SUN_ICON_Y, containerW, containerH, TFT_YELLOW);
    lcd.drawRect(SUN_ICON_X, SUN_ICON_Y, containerW, containerH, TFT_BLACK);
    lcd.fillRect(SUN_ICON_X + 2, SUN_ICON_Y + 2, 40, 40, TFT_BLACK);
    lcd.drawBitmap(SUN_ICON_X + 2, SUN_ICON_Y + 2, sunIcon, 40, 40, TFT_YELLOW);

    float totalPvPower = (dcPvPower + acPvPower[0] + acPvPower[1] + acPvPower[2]) / 1000.0;
    String pvText = String(totalPvPower, 1) + " kW";
    lcd.setTextColor(TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    lcd.setCursor(SUN_ICON_X + 22, SUN_ICON_Y + 3 * (containerH / 4) - (lcd.fontHeight() / 2));
    lcd.print(pvText);
}

void drawTabButtons() {
    lcd.fillRect(439, 1, TAB1_BUTTON_W, 319, TFT_WHITE);
    const struct { int x, y; const char* label; } tabs[] = {
        {TAB1_BUTTON_X, TAB1_BUTTON_Y, "1"},
        {TAB2_BUTTON_X, TAB2_BUTTON_Y, "2"},
        {TAB3_BUTTON_X, TAB3_BUTTON_Y, "3"},
    };
    for (auto& t : tabs) {
        lcd.fillRect(t.x, t.y, TAB1_BUTTON_W, TAB1_BUTTON_H, TFT_GREY);
        lcd.drawRect(t.x, t.y, TAB1_BUTTON_W, TAB1_BUTTON_H, TFT_BLACK);
        lcd.setTextSize(1); lcd.setTextColor(TFT_BLACK);
        int tw = lcd.textWidth(t.label);
        lcd.setCursor(t.x + (TAB1_BUTTON_W - tw) / 2, t.y + (TAB1_BUTTON_H - lcd.fontHeight()) / 2);
        lcd.print(t.label);
    }
}

void drawCarConnectionButton() {
    uint16_t iconColor = (chargerStatus != 0) ? TFT_GREEN : TFT_GREY;
    lcd.fillRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
    lcd.drawBitmap(CAR_ICON_X, CAR_ICON_Y, carIcon, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, iconColor);
    lcd.drawRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
}

void drawSOCThreshold() {
    uint16_t color;
    switch (rectangleState) {
        case GREEN:  SOC_THRESHOLD = 80;  color = TFT_GREEN; break;
        case YELLOW: SOC_THRESHOLD = 90;  color = TFT_YELLOW; break;
        case RED:    SOC_THRESHOLD = 100; color = TFT_RED; break;
    }
    lcd.fillRect(SOC_RECT_X, SOC_RECT_Y, SOC_RECT_SIZE, SOC_RECT_SIZE, color);
    lcd.drawRect(SOC_RECT_X, SOC_RECT_Y, SOC_RECT_SIZE, SOC_RECT_SIZE, TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    String text = String((int)SOC_THRESHOLD) + "%";
    int tw = lcd.textWidth(text);
    int tx = SOC_RECT_X + (SOC_RECT_SIZE - tw) / 2;
    int ty = SOC_RECT_Y + (SOC_RECT_SIZE - lcd.fontHeight()) / 2;
    lcd.setTextColor(TFT_BLACK);
    lcd.setCursor(tx, ty);
    lcd.print(text);
}

void drawChargeModeButton() {
    uint16_t bg = lcd.color565(173, 216, 230);
    lcd.fillRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, bg);
    lcd.drawRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, TFT_BLACK);
    lcd.setTextColor(TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    String modeText;
    switch (chargeMode) {
        case 0: modeText = "Manuell"; break;
        case 1: modeText = "Auto"; break;
        case 2: modeText = "Zeitplan"; break;
        default: modeText = "?"; break;
    }
    float powerKW = chargePower / 1000.0;
    String powerText = String(powerKW, 1) + " kW";

    int mw = lcd.textWidth(modeText);
    lcd.setCursor(BUTTON_X + (BUTTON_W - mw) / 2, BUTTON_Y + BUTTON_H / 4 - lcd.fontHeight() / 2);
    lcd.print(modeText);

    int pw = lcd.textWidth(powerText);
    lcd.setCursor(BUTTON_X + (BUTTON_W - pw) / 2, BUTTON_Y + 3 * BUTTON_H / 4 - lcd.fontHeight() / 2);
    lcd.print(powerText);
}

void drawStartStopButton() {
    uint16_t color = (startStopCharging == 1) ? TFT_GREEN : TFT_RED;
    String text = (startStopCharging == 1) ? "An" : "Aus";
    lcd.fillRect(START_STOP_RECT_X, START_STOP_RECT_Y, START_STOP_RECT_SIZE, START_STOP_RECT_SIZE, color);
    lcd.drawRect(START_STOP_RECT_X, START_STOP_RECT_Y, START_STOP_RECT_SIZE, START_STOP_RECT_SIZE, TFT_BLACK);
    lcd.setTextColor(TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    int tw = lcd.textWidth(text);
    lcd.setCursor(START_STOP_RECT_X + (START_STOP_RECT_SIZE - tw) / 2,
                  START_STOP_RECT_Y + (START_STOP_RECT_SIZE - lcd.fontHeight()) / 2);
    lcd.print(text);
}

void drawCarRangeButton() {
    const float maxCapacity = 32.3;
    const float consumptionPer100Km = 13.5;
    float range = (socValue / 100.0) * (maxCapacity / consumptionPer100Km);

    lcd.fillRect(CAR_RANGE_X, CAR_RANGE_Y, CAR_RANGE_WIDTH, CAR_RANGE_HEIGHT, TFT_LIGHTGREY);
    lcd.drawRect(CAR_RANGE_X, CAR_RANGE_Y, CAR_RANGE_WIDTH, CAR_RANGE_HEIGHT, TFT_BLACK);

    String text = String(range, 0) + " km";
    lcd.setTextColor(TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    int tw = lcd.textWidth(text);
    lcd.setCursor(CAR_RANGE_X + (CAR_RANGE_WIDTH - tw) / 2,
                  CAR_RANGE_Y + (CAR_RANGE_HEIGHT - lcd.fontHeight()) / 2);
    lcd.print(text);
}

void drawManualModePhaseButton() {
    uint16_t bg = (manualModePhase == 0) ? TFT_GREEN : TFT_YELLOW;
    lcd.fillRect(MANUAL_MODE_PHASE_RECT_X, MANUAL_MODE_PHASE_RECT_Y,
                 MANUAL_MODE_PHASE_RECT_W, MANUAL_MODE_PHASE_RECT_H, bg);
    lcd.drawRect(MANUAL_MODE_PHASE_RECT_X, MANUAL_MODE_PHASE_RECT_Y,
                 MANUAL_MODE_PHASE_RECT_W, MANUAL_MODE_PHASE_RECT_H, TFT_BLACK);
    String text = (manualModePhase == 0) ? "2 P" : "1 P";
    int tw = lcd.textWidth(text);
    lcd.setCursor(MANUAL_MODE_PHASE_RECT_X + (MANUAL_MODE_PHASE_RECT_W - tw) / 2,
                  MANUAL_MODE_PHASE_RECT_Y + (MANUAL_MODE_PHASE_RECT_H - lcd.fontHeight()) / 2);
    lcd.print(text);
}

void drawCarIconWithSOC() {
    uint16_t iconColor = (chargerStatus != 0) ? TFT_GREEN : lcd.color565(173, 216, 230);
    lcd.fillRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
    lcd.drawBitmap(CAR_ICON_X, CAR_ICON_Y, carIcon, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, iconColor);
    lcd.drawRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);

    int socPercentage = socValue / 100;
    int rectX = CAR_ICON_X + 36, rectY = CAR_ICON_Y + 22;
    int rectW = 50, rectH = 30;
    lcd.fillRect(rectX, rectY, rectW, rectH, iconColor);
    lcd.setTextColor(TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    String text = String(socPercentage) + "%";
    int tw = lcd.textWidth(text);
    lcd.setCursor(rectX + (rectW - tw) / 2, rectY + (rectH - lcd.fontHeight()) / 2);
    lcd.print(text);
}

void drawWaterTempButton() {
    lcd.fillRect(H2O_RECT_X, H2O_RECT_Y, H2O_RECT_SIZE, H2O_RECT_SIZE, TFT_BLUE);
    lcd.drawRect(H2O_RECT_X, H2O_RECT_Y, H2O_RECT_SIZE, H2O_RECT_SIZE, TFT_BLACK);

    float tempC = waterTemperature / 100.0;
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    String text = String(tempC, 0);
    int tw = lcd.textWidth(text);
    int tx = H2O_RECT_X + (H2O_RECT_SIZE - tw - 26) / 2;
    int ty = H2O_RECT_Y + (H2O_RECT_SIZE - lcd.fontHeight()) / 2;
    lcd.setCursor(tx, ty);
    lcd.print(text);
    lcd.drawCircle(tx + tw + 8, ty - 1, 3, TFT_WHITE);
    lcd.setCursor(tx + tw + 12, ty);
    lcd.print("C");
}

void drawHouseIcon() {
    lcd.fillRect(HOUSE_ICON_X, HOUSE_ICON_Y, HOUSE_ICON_WIDTH, HOUSE_ICON_HEIGHT, TFT_BLACK);
    lcd.drawBitmap(HOUSE_ICON_X, HOUSE_ICON_Y, houseIcon, HOUSE_ICON_WIDTH, HOUSE_ICON_HEIGHT, TFT_GREEN);
    lcd.fillRect(HOUSE_ICON_X + HOUSE_ICON_WIDTH, HOUSE_ICON_Y, 80, HOUSE_ICON_HEIGHT, TFT_GREEN);
    lcd.drawRect(HOUSE_ICON_X, HOUSE_ICON_Y, HOUSE_ICON_WIDTH + 80, HOUSE_ICON_HEIGHT, TFT_BLACK);
}

void drawGridPowerButton() {
    lcd.fillRect(GRID_ICON_X, GRID_ICON_Y, GRID_ICON_WIDTH, GRID_ICON_HEIGHT, TFT_BLUE);
    lcd.drawRect(GRID_ICON_X, GRID_ICON_Y, GRID_ICON_WIDTH, GRID_ICON_HEIGHT, TFT_BLACK);
    String text = String(totalGridPowerKW, 2) + " kW";
    lcd.setTextColor(TFT_WHITE);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    int tw = lcd.textWidth(text);
    lcd.setCursor(GRID_ICON_X + (GRID_ICON_WIDTH - tw) / 2,
                  GRID_ICON_Y + (GRID_ICON_HEIGHT - lcd.fontHeight()) / 2);
    lcd.print(text);
}

void drawPylontechSOCWithPower() {
    char socStr[10];
    snprintf(socStr, sizeof(socStr), "%.0f %%", (float)PylontechSOC);
    int16_t signedPower = (int16_t)batteryPower;
    float powerKW = signedPower / 1000.0;
    char powerStr[20];
    snprintf(powerStr, sizeof(powerStr), "%.1fkW", powerKW);

    int rx = HOUSE_ICON_X + HOUSE_ICON_WIDTH;
    int ry = HOUSE_ICON_Y;
    int rw = 80, rh = HOUSE_ICON_HEIGHT;

    lcd.setTextColor(TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    int sw = lcd.textWidth(socStr);
    lcd.setCursor(rx + (rw - sw) / 2, ry + rh / 4 - lcd.fontHeight() / 2);
    lcd.print(socStr);

    int pw = lcd.textWidth(powerStr);
    lcd.setCursor(rx + (rw - pw) / 2, ry + 3 * rh / 4 - lcd.fontHeight() / 2);
    lcd.print(powerStr);
}

void drawClockTab() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[16];
        strftime(timeStr, sizeof(timeStr), "%H:%M Uhr", &timeinfo);
        lcd.setTextColor(TFT_BLACK);
        lcd.setTextSize(2);
        int tw = lcd.textWidth(timeStr);
        lcd.setCursor((TAB1_BUTTON_X - tw) / 2, (TFT_HEIGHT - lcd.fontHeight()) / 2);
        lcd.print(timeStr);
    }
}

// ============================================================
// Display Data (called with LCD_LOCK held)
// ============================================================
void displayData() {
    if (currentTab == 1) {
        drawCarRangeButton();
        drawSOCThreshold();
        drawChargeModeButton();
        drawStartStopButton();
        drawCarConnectionButton();
        drawCarIconWithSOC();
        drawManualModePhaseButton();
        drawWaterTempButton();
        drawSunIcon();
        drawTibberPrice();
        drawHouseIcon();
        drawPylontechSOCWithPower();
        drawGridPowerButton();
    } else if (currentTab == 2) {
        lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
    } else if (currentTab == 3) {
        lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
        drawTibberPriceGraph(tibberPrices, 48);
    }
}

void switchTab(int tab) {
    currentTab = tab;
    lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
    drawTabButtons();
    drawBrightnessButton();
    lcd.setTextColor(TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    if (currentTab == 1) {
        displayData();
    } else if (currentTab == 2) {
        drawClockTab();
        drawBoilerSwitch();
    } else if (currentTab == 3) {
        displayData();
    }
}

// ============================================================
// Touch helpers
// ============================================================
bool isWithinButton(int32_t tx, int32_t ty, int32_t bx, int32_t by, int32_t bw, int32_t bh) {
    return (tx >= bx && tx <= bx + bw && ty >= by && ty <= by + bh);
}

// ============================================================
// WiFi
// ============================================================
void connectToWiFi() {
    IPAddress localIP(STATIC_IP);
    IPAddress gateway(GATEWAY_IP);
    IPAddress subnet(SUBNET_MASK);
    IPAddress dns1(PRIMARY_DNS);
    IPAddress dns2(SECONDARY_DNS);

    WiFi.config(localIP, gateway, subnet, dns1, dns2);

    Serial.print("Connecting to primary WiFi...");
    WiFi.begin(PRIMARY_SSID, PRIMARY_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nTrying secondary WiFi...");
        WiFi.begin(SECONDARY_SSID, SECONDARY_PASSWORD);
        attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost. Reconnecting...");
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(1000));
        connectToWiFi();
        // Mark Modbus connections as lost so modbusTask reconnects
        socConnected = false;
        evcsConnected = false;
        cerboConnected = false;
    }
}

// ============================================================
// Display power
// ============================================================
void adjustBrightness(int level) {
    brightnessLevel = constrain(level, 0, 255);
    lcd.setBrightness(brightnessLevel);
}

void turnOffDisplay() {
    if (displayOn) {
        lcd.setBrightness(0);
        displayOn = false;
        Serial.println("Display off (inactivity).");
    }
}

void turnOnDisplay() {
    if (!displayOn) {
        adjustBrightness(brightnessLevel);
        displayOn = true;
        if (LCD_LOCK()) {
            switchTab(1);
            LCD_UNLOCK();
        }
        Serial.println("Display on (touch).");
    }
}

void autoAdjustBrightness() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    adjustBrightness((timeinfo.tm_hour >= 20 || timeinfo.tm_hour < 6) 
                      ? BRIGHTNESS_NIGHT : BRIGHTNESS_DAY);
}

// ============================================================
// FreeRTOS Tasks
// ============================================================
void modbusTask(void *parameter) {
    // Add this task to watchdog
    esp_task_wdt_add(NULL);

    while (true) {
        esp_task_wdt_reset();

        // --- EVCS ---
        if (!evcsConnected) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            evcsConnected = connectModbusServer(remoteEVCS, 2);
            xSemaphoreGive(modbusMutex);
        }
        if (evcsConnected) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteEVCS, MANUAL_MODE_PHASE_REG, manualModePhase);
            readModbusData(remoteEVCS, CHARGE_MODE_REG, chargeMode);
            readModbusData(remoteEVCS, CHARGE_POWER_REG, chargePower);
            readModbusData(remoteEVCS, START_STOP_CHARGING_REG, startStopCharging);
            readModbusData(remoteEVCS, CHARGER_STATUS_REG, chargerStatus);
            xSemaphoreGive(modbusMutex);
        }

        esp_task_wdt_reset();

        // --- SOC ---
        if (!socConnected) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            socConnected = connectModbusServer(remoteSOC, 2);
            xSemaphoreGive(modbusMutex);
        }
        if (socConnected) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteSOC, SOC_REG, socValue);
            readModbusData(remoteSOC, TIMESTAMP_HIGH_REG, timestampHigh);
            readModbusData(remoteSOC, TIMESTAMP_LOW_REG, timestampLow);
            xSemaphoreGive(modbusMutex);

            // SOC threshold check
            if (socValue > SOC_THRESHOLD * 100 && startStopCharging == 1) {
                Serial.println("SOC over threshold. Stopping charging.");
                startStopCharging = 0;
                queueModbusWrite(remoteEVCS, START_STOP_CHARGING_REG, 0);
            }
        }

        esp_task_wdt_reset();

        // --- CERBO GX ---
        if (!cerboConnected) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            cerboConnected = connectModbusServer(remoteCERBO, 2);
            xSemaphoreGive(modbusMutex);
        }
        if (cerboConnected) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteCERBO, PYLONTECH_SOC_REG, PylontechSOC, CERBO_UNIT_ID_VAL);
            readModbusData(remoteCERBO, WATER_TEMP_REG, waterTemperature, CERBO_UNIT_ID_TEMP_VAL);
            readModbusData(remoteCERBO, 842, batteryPower, CERBO_UNIT_ID_VAL);
            readModbusData(remoteCERBO, DC_PV_POWER_REG, dcPvPower, CERBO_UNIT_ID_VAL);
            for (int i = 0; i < 3; i++) {
                readModbusData(remoteCERBO, AC_PV_POWER_REGS[i], acPvPower[i], CERBO_UNIT_ID_VAL);
            }
            readModbusData(remoteCERBO, GRID_PHASE1_REG, rawgridPhase1, CERBO_UNIT_ID_VAL);
            readModbusData(remoteCERBO, GRID_PHASE2_REG, rawgridPhase2, CERBO_UNIT_ID_VAL);
            readModbusData(remoteCERBO, GRID_PHASE3_REG, rawgridPhase3, CERBO_UNIT_ID_VAL);

            totalGridPowerKW = ((int16_t)rawgridPhase1 + (int16_t)rawgridPhase2 + (int16_t)rawgridPhase3) / 1000.0;
            xSemaphoreGive(modbusMutex);
        }

        // Update display (with LCD mutex)
        if (displayOn) {
            if (LCD_LOCK()) {
                displayData();
                LCD_UNLOCK();
            }
        }

        int interval = immediateModbusRequest ? MODBUS_SHORT_INTERVAL : MODBUS_LONG_INTERVAL;
        immediateModbusRequest = false;
        vTaskDelay(pdMS_TO_TICKS(interval));
    }
}

void modbusWriteTask(void *parameter) {
    esp_task_wdt_add(NULL);
    ModbusWriteRequest request;

    while (true) {
        esp_task_wdt_reset();
        // Use shorter timeout so WDT gets reset more often
        if (xQueueReceive(modbusWriteQueue, &request, pdMS_TO_TICKS(2000))) {
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            bool ok = writeModbusData(request.server, request.reg, request.value, request.unitID);
            xSemaphoreGive(modbusMutex);
            Serial.printf("Modbus write %s: reg=%d val=%d\n", ok ? "OK" : "FAIL", request.reg, request.value);
        }
    }
}

void touchTask(void *parameter) {
    esp_task_wdt_add(NULL);
    int32_t x, y;

    while (true) {
        esp_task_wdt_reset();

        if (lcd.getTouch(&x, &y)) {
            lastInteractionTime = millis();
            turnOnDisplay();

            if (displayOn) {
                unsigned long now = millis();
                if (now - lastTouchTime > debounceDelay) {
                    lastTouchTime = now;
                    immediateModbusRequest = true;

                    if (LCD_LOCK()) {
                        if (currentTab == 1) {
                            if (isWithinButton(x, y, BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
                                chargeMode = (chargeMode + 1) % 3;
                                drawChargeModeButton();
                                queueModbusWrite(remoteEVCS, CHARGE_MODE_REG, chargeMode);
                            }
                            else if (isWithinButton(x, y, START_STOP_RECT_X, START_STOP_RECT_Y, START_STOP_RECT_SIZE, START_STOP_RECT_SIZE)) {
                                startStopCharging = (startStopCharging == 1) ? 0 : 1;
                                drawStartStopButton();
                                queueModbusWrite(remoteEVCS, START_STOP_CHARGING_REG, startStopCharging);
                            }
                            else if (isWithinButton(x, y, SOC_RECT_X, SOC_RECT_Y, SOC_RECT_SIZE, SOC_RECT_SIZE)) {
                                rectangleState = (rectangleState == GREEN) ? YELLOW : (rectangleState == YELLOW) ? RED : GREEN;
                                drawSOCThreshold();
                            }
                            else if (isWithinButton(x, y, MANUAL_MODE_PHASE_RECT_X, MANUAL_MODE_PHASE_RECT_Y, MANUAL_MODE_PHASE_RECT_W, MANUAL_MODE_PHASE_RECT_H)) {
                                manualModePhase = (manualModePhase + 1) % 2;
                                drawManualModePhaseButton();
                                queueModbusWrite(remoteEVCS, MANUAL_MODE_PHASE_REG, manualModePhase);
                            }
                            else if (isWithinButton(x, y, TIBBER_RECT_X, TIBBER_RECT_Y, TIBBER_RECT_SIZE, TIBBER_RECT_SIZE)) {
                                switchTab(3);
                            }
                        }

                        if (currentTab == 2) {
                            if (isWithinButton(x, y, BOILER_SWITCH_X, BOILER_SWITCH_Y, BOILER_SWITCH_W, BOILER_SWITCH_H)) {
                                LCD_UNLOCK();
                                toggleBoilerMode();  // takes modbusMutex internally
                                if (LCD_LOCK()) {
                                    drawBoilerSwitch();
                                    LCD_UNLOCK();
                                }
                                goto skipUnlock;
                            }
                        }

                        // Global buttons
                        if (isWithinButton(x, y, BRIGHTNESS_RECT_X, BRIGHTNESS_RECT_Y, BRIGHTNESS_RECT_W, BRIGHTNESS_RECT_H)) {
                            brightnessLevel = (brightnessLevel == 255) ? BRIGHTNESS_NIGHT : BRIGHTNESS_DAY;
                            adjustBrightness(brightnessLevel);
                        }
                        else if (isWithinButton(x, y, TAB1_BUTTON_X, TAB1_BUTTON_Y, TAB1_BUTTON_W, TAB1_BUTTON_H)) {
                            switchTab(1);
                        }
                        else if (isWithinButton(x, y, TAB2_BUTTON_X, TAB2_BUTTON_Y, TAB2_BUTTON_W, TAB2_BUTTON_H)) {
                            switchTab(2);
                        }
                        else if (isWithinButton(x, y, TAB3_BUTTON_X, TAB3_BUTTON_Y, TAB3_BUTTON_W, TAB3_BUTTON_H)) {
                            switchTab(3);
                        }

                        LCD_UNLOCK();
                        skipUnlock:;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println("WT32 Tibber Display v10 (stable)");

    // Initialize watchdog (60s timeout) - may already be initialized by bootloader
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
    if (wdt_err == ESP_ERR_INVALID_STATE) {
        // Already initialized, reconfigure
        esp_task_wdt_reconfigure(&wdt_config);
    }
    esp_task_wdt_add(NULL);  // Add loop() task

    // Create mutexes
    modbusMutex = xSemaphoreCreateMutex();
    lcdMutex = xSemaphoreCreateMutex();
    modbusWriteQueue = xQueueCreate(10, sizeof(ModbusWriteRequest));

    // WiFi
    connectToWiFi();

    // NTP
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
        Serial.println(&timeinfo, "Time: %Y-%m-%d %H:%M:%S");
    }

    // OTA
    ArduinoOTA.setHostname("WT32-OTA-Display");
    ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
    ArduinoOTA.onEnd([]() { Serial.println("OTA Done"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA: %u%%\r", progress / (total / 100));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]\n", error);
    });
    ArduinoOTA.begin();

    // Display
    lcd.init();
    if (lcd.width() < lcd.height()) lcd.setRotation(lcd.getRotation() ^ 1);
    lcd.fillScreen(TFT_WHITE);
    lcd.setTextColor(TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    autoAdjustBrightness();
    drawWiFiIcon(WiFi.status() == WL_CONNECTED);

    // Tibber
    fetchTibberPrices();
    drawTibberPrice();

    // Initial display
    switchTab(1);

    // Modbus client
    mb.client();

    // Start tasks with proper stack sizes
    xTaskCreatePinnedToCore(modbusTask, "Modbus", MODBUS_TASK_STACK, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(modbusWriteTask, "ModbusWr", MODBUS_WRITE_STACK, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(touchTask, "Touch", TOUCH_TASK_STACK, NULL, 2, NULL, 1);

    lastInteractionTime = millis();
    Serial.println("Setup complete.");
}

// ============================================================
// Loop
// ============================================================
void loop() {
    esp_task_wdt_reset();

    ArduinoOTA.handle();

    // Display timeout
    if (displayOn && (millis() - lastInteractionTime > TFT_OFF_DELAY)) {
        turnOffDisplay();
    }

    // WiFi check (non-blocking, every 30s)
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = millis();
        checkWiFiConnection();
        if (displayOn && LCD_LOCK()) {
            drawWiFiIcon(WiFi.status() == WL_CONNECTED);
            LCD_UNLOCK();
        }
    }

    // Midnight price shift
    static int lastCheckedHour = -1;
    int currentHour = getCurrentHour();
    if (currentHour == 0 && lastCheckedHour != 0) {
        shiftTibberPrices();
    }
    lastCheckedHour = currentHour;

    // Hourly price update
    static int lastUpdatedHour = -1;
    if (currentHour != lastUpdatedHour) {
        lastUpdatedHour = currentHour;
        updateCurrentElectricityPrice();
        if (displayOn && LCD_LOCK()) {
            drawTibberPrice();
            LCD_UNLOCK();
        }
    }

    // Daily price fetch
    checkAndFetchTibberPrices();

    vTaskDelay(pdMS_TO_TICKS(100));
}
