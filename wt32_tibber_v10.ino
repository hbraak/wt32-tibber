#include <LovyanGFX.hpp>    // main library
#include <WiFi.h>
#include <ModbusTCP.h>
#include <TimeLib.h>
#include "globals.h"         // Für die globalen Variablen
#include "modbus_helpers.h"  // Für die Modbus-Funktionen
#include "tibber.h"  // Einbinden der Tibber-Funktionalität
#include "boiler.h"  // Boilersteuerungs-Funktionen einbinden
#include <HTTPClient.h>

//OTA
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//OTA

// Display Configuration
#define TFT_WIDTH   320
#define TFT_HEIGHT  480
#define TFT_GREY 0x5AEB
#define TFT_OFF_DELAY 120000  // 2 Minuten

// Increase stack size for tasks
#define TASK_STACK_SIZE 8192

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel_instance;  // ST7796UI
  lgfx::Bus_Parallel8 _bus_instance;    // MCU8080 8B
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_FT5x06  _touch_instance;

public:
  LGFX(void) {
    // Bus Configuration
    auto cfg = _bus_instance.config();
    cfg.freq_write = 40000000;
    cfg.pin_wr = 47;
    cfg.pin_rd = -1;
    cfg.pin_rs = 0;
    cfg.pin_d0 = 9;
    cfg.pin_d1 = 46;
    cfg.pin_d2 = 3;
    cfg.pin_d3 = 8;
    cfg.pin_d4 = 18;
    cfg.pin_d5 = 17;
    cfg.pin_d6 = 16;
    cfg.pin_d7 = 15;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);

    // Panel Configuration
    auto cfg_panel = _panel_instance.config();
    cfg_panel.pin_cs = -1;
    cfg_panel.pin_rst = 4;
    cfg_panel.pin_busy = -1;
    cfg_panel.panel_width = TFT_WIDTH;
    cfg_panel.panel_height = TFT_HEIGHT;
    cfg_panel.offset_x = 0;
    cfg_panel.offset_y = 0;
    cfg_panel.offset_rotation = 0;
    cfg_panel.dummy_read_pixel = 8;
    cfg_panel.dummy_read_bits = 1;
    cfg_panel.readable = false;
    cfg_panel.invert = true;
    cfg_panel.rgb_order = false;
    cfg_panel.dlen_16bit = false;
    cfg_panel.bus_shared = false;
    _panel_instance.config(cfg_panel);

    // Light Configuration
    auto cfg_light = _light_instance.config();
    cfg_light.pin_bl = 45;
    cfg_light.invert = false;
    cfg_light.freq = 44100;
    cfg_light.pwm_channel = 7;
    _light_instance.config(cfg_light);
    _panel_instance.setLight(&_light_instance);

    // Touch Configuration
    auto cfg_touch = _touch_instance.config();
    cfg_touch.x_min = 0;
    cfg_touch.x_max = 319;
    cfg_touch.y_min = 0;
    cfg_touch.y_max = 479;
    cfg_touch.pin_int = 7;
    cfg_touch.bus_shared = true;
    cfg_touch.offset_rotation = 0;
    cfg_touch.i2c_port = 1;
    cfg_touch.i2c_addr = 0x38;
    cfg_touch.pin_sda = 6;
    cfg_touch.pin_scl = 5;
    cfg_touch.freq = 400000;
    _touch_instance.config(cfg_touch);
    _panel_instance.setTouch(&_touch_instance);

    setPanel(&_panel_instance);
  }
};

static LGFX lcd; // declare display variable

// Primary WiFi credentials
const char* primarySSID = "FRITZ!Box Fon WLAN 7390";
const char* primaryPassword = "4551516174768576";

// Secondary WiFi credentials
const char* secondarySSID = "SLC";
const char* secondaryPassword = "82603690157953239701";
// Modbus Configuration
ModbusTCP mb;
const int SOC_REG = 1;
const int TIMESTAMP_HIGH_REG = 2;
const int TIMESTAMP_LOW_REG = 3;
IPAddress remoteSOC(192, 168, 178, 121);

const int CHARGE_POWER_REG = 5014;
const int CHARGER_STATUS_REG = 5015;
const int CHARGER_AUTOSTART_REG = 5049;
const int MANUAL_MODE_PHASE_REG = 5055;
const int AUTO_MODE_PHASE_REG = 5056;
const int CHARGE_MODE_REG = 5009;
const int START_STOP_CHARGING_REG = 5010;
IPAddress remoteEVCS(192, 168, 178, 78);

// New Modbus Configuration for CERBO GX

const int PYLONTECH_SOC_REG = 843;  // Modbus register for Pylontech SOC
const int CERBO_UNIT_ID_TEMP = 24; 
const int DC_PV_POWER_REG = 850; // DC-coupled PV power (INT32)
const int AC_PV_POWER_REGS[] = {811, 812, 813}; // AC-coupled PV power phases (INT32)
// Modbus-Registeradressen für die Relais zur Boilersteuerung
const int RELAY1_REG = 806;  // Registeradresse für das erste Relais
const int RELAY2_REG = 807;  // Registeradresse für das zweite Relais
// Variablen für die Modbusregister der GRID-Leistung
const int GRID_PHASE1_REG = 820;  // register für Phase 1
const int GRID_PHASE2_REG = 821;  // register für Phase 2
const int GRID_PHASE3_REG = 822;  // register für Phase 3
const int WATER_TEMP_REG = 3304;  // Modbus register for water temperature

uint16_t gridPhase1 = 0;
uint16_t gridPhase2 = 0;
uint16_t gridPhase3 = 0;
uint16_t rawgridPhase1 = 0;
uint16_t rawgridPhase2 = 0;
uint16_t rawgridPhase3 = 0;
float totalGridPowerKW = 0.0;  // This will store the total grid power in kW

uint16_t PylontechSOC = 0;



uint16_t manualModePhase = 0;  // Global variable for phase control
uint16_t waterTemperature = 0;    // Variable to store water temperature
uint16_t dcPvPower = 0;
uint16_t acPvPower[3] = {0};
uint16_t batteryPower = 0;  // Variable für Batterieleistung (Watt)

// Variables for Modbus data
uint16_t socValue = 0;
uint16_t chargeMode = 0;
uint16_t startStopCharging = 0;
uint16_t timestampHigh = 0;
uint16_t timestampLow = 0;
uint32_t timestamp = 0;
uint16_t chargePower = 0;
uint16_t chargerStatus = 0;
int boilerMode = 0;  // 0 = Auto, 1 = 2kW, 2 = 4kW, 3 = 6kW

// Previous Values to Avoid Flicker
uint16_t oldSocValue = 0;
uint16_t oldChargeMode = 9;
uint16_t oldStartStopCharging = 9;
uint16_t oldChargerStatus = 24;  // Wird initial gezeichnet
uint16_t oldManualModePhase = 0;
uint16_t previousWaterTemperature = 0;
float previousTotalPvPower = 0.0;

// Rectangle State Enum
enum RectangleState { GREEN, YELLOW, RED };
RectangleState rectangleState = GREEN;  // Initial state
RectangleState oldRectangleState = GREEN;

float SOC_THRESHOLD = 80.0; // SOC Threshold to stop charging

// Timeout tracking for display
unsigned long lastInteractionTime = 0;
bool displayOn = true;

int brightnessLevel = 255; // Variable for brightness level
unsigned long lastTouchTime = 0; // Added to fix undefined variable bug
unsigned long debounceDelay = 200; // Example debounce delay
int currentTab = 1; // Variable for current tab
volatile bool immediateModbusRequest = false; // Added to fix undefined variable bug
static unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 10000; // Alle 5 Sekunden prüfen
// Mutex for Modbus data access
SemaphoreHandle_t modbusMutex;

// Queue for Modbus write requests
QueueHandle_t modbusWriteQueue;

// Structure for Modbus write requests
struct ModbusWriteRequest {
  IPAddress server;
  int reg;
  uint16_t value;
  uint8_t unitID;  // Added for Unit ID
};


bool socConnected = false;
bool evcsConnected = false;
bool cerboConnected = false;  // New variable for CERBO GX connection

const int BORDER = 10;
const int ICON_HEIGHT = 70;
const int ICON_WIDTH1 = 102;
const int ICON_WIDTH0 = 70;
const int ICON_WIDTH2 = 150;
const int SPACE = 5;
// Function prototypes
void readModbusData(IPAddress server, int reg, uint16_t &value, uint8_t unitID = MODBUSIP_UNIT);
void adjustBrightness(int level);
void autoAdjustBrightness();

#define BOILER_SWITCH_X BORDER
#define BOILER_SWITCH_Y BORDER
#define BOILER_SWITCH_W 120
#define BOILER_SWITCH_H 40

// Constants for Button Positions and Sizes
#define CAR_RANGE_X BORDER
#define CAR_RANGE_Y BORDER + ICON_HEIGHT + SPACE
#define CAR_RANGE_WIDTH ICON_WIDTH1
#define CAR_RANGE_HEIGHT ICON_HEIGHT

#define CAR_ICON_X BORDER
#define CAR_ICON_Y BORDER
#define CAR_ICON_WIDTH ICON_WIDTH1
#define CAR_ICON_HEIGHT ICON_HEIGHT

#define HOUSE_ICON_X BORDER +ICON_WIDTH0 + ICON_WIDTH1 + SPACE + SPACE
#define HOUSE_ICON_Y BORDER +  SPACE + 2*ICON_HEIGHT + SPACE
#define HOUSE_ICON_WIDTH ICON_WIDTH0
#define HOUSE_ICON_HEIGHT ICON_HEIGHT

// Tibber
#define TIBBER_RECT_X BORDER +ICON_WIDTH1 +  SPACE
#define TIBBER_RECT_Y BORDER + SPACE + 2* ICON_HEIGHT + SPACE
#define TIBBER_RECT_SIZE ICON_HEIGHT

// Water Temperature
#define H2O_RECT_X BORDER +ICON_WIDTH0 + ICON_WIDTH1 + ICON_WIDTH2 + SPACE + SPACE + SPACE
#define H2O_RECT_Y BORDER +  SPACE + 2*ICON_HEIGHT + SPACE
#define H2O_RECT_SIZE ICON_HEIGHT

// Button SOC-Threshold
#define SOC_RECT_X BORDER +ICON_WIDTH0 + ICON_WIDTH1 + ICON_WIDTH2 + SPACE + SPACE + SPACE
#define SOC_RECT_Y BORDER
#define SOC_RECT_SIZE ICON_HEIGHT

// Button StartStopCharging
#define START_STOP_RECT_X BORDER +SPACE + ICON_WIDTH1
#define START_STOP_RECT_Y BORDER
#define START_STOP_RECT_SIZE ICON_HEIGHT

// Button coordinates charge Mode
#define BUTTON_X BORDER +ICON_WIDTH0 + ICON_WIDTH1 + SPACE + SPACE 
#define BUTTON_Y BORDER
#define BUTTON_W ICON_WIDTH2
#define BUTTON_H ICON_HEIGHT

// Button MANUAL_MODE_PHASE
#define MANUAL_MODE_PHASE_RECT_X BORDER +SPACE + ICON_WIDTH1
#define MANUAL_MODE_PHASE_RECT_Y BORDER + ICON_HEIGHT + SPACE
#define MANUAL_MODE_PHASE_RECT_W ICON_HEIGHT
#define MANUAL_MODE_PHASE_RECT_H ICON_HEIGHT

// Button Brightness Control
#define BRIGHTNESS_RECT_X 439
#define BRIGHTNESS_RECT_Y 1
#define BRIGHTNESS_RECT_W 40
#define BRIGHTNESS_RECT_H 40

// Button Tab Control
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

#define WIFI_ICON_X (439)  // X-Position für das WLAN-Symbol (rechte obere Ecke)
#define WIFI_ICON_Y (280)  // Y-Position für das WLAN-Symbol
#define WIFI_ICON_SIZE 40  // Größe des Symbols

#include "carIcon.h"  // Include your car icon bitmap here
#include "houseIcon.h"  // Include your car icon bitmap here
#include "sunicon.h"  // Include your car icon bitmap here

void drawWiFiIcon(bool connected) {
    // Setze die Farbe des Hintergrundquadrats basierend auf der Verbindung
    uint16_t backgroundColor = connected ? TFT_GREEN : TFT_LIGHTGREY;

    // Zeichne das Quadrat für das WLAN-Symbol
    lcd.fillRect(WIFI_ICON_X, WIFI_ICON_Y, WIFI_ICON_SIZE, WIFI_ICON_SIZE, backgroundColor);
    lcd.drawRect(WIFI_ICON_X, WIFI_ICON_Y, WIFI_ICON_SIZE, WIFI_ICON_SIZE, TFT_BLACK);

    // Koordinaten für das Zentrum des Symbols
    int centerX = WIFI_ICON_X + WIFI_ICON_SIZE / 2;
    int centerY = WIFI_ICON_Y + WIFI_ICON_SIZE / 2 + 10;

    // Zeichne die einzelnen Bögen des WLAN-Symbols in Weiß
    uint16_t iconColor = TFT_BLACK;

    // Erster (größter) Bogen
    lcd.drawArc(centerX, centerY, WIFI_ICON_SIZE / 2, WIFI_ICON_SIZE / 2, 225, 315, iconColor);

    // Zweiter Bogen
    lcd.drawArc(centerX, centerY, WIFI_ICON_SIZE / 3, WIFI_ICON_SIZE / 3, 225, 315, iconColor);

    // Kleinster Punkt (Mitte, wenn verbunden)
    if (connected) {
        lcd.fillCircle(centerX, centerY, 3, iconColor);
    }
}


void drawBoilerSwitch() {
    lcd.fillRect(BOILER_SWITCH_X, BOILER_SWITCH_Y, BOILER_SWITCH_W, BOILER_SWITCH_H, TFT_GREY);
    lcd.drawRect(BOILER_SWITCH_X, BOILER_SWITCH_Y, BOILER_SWITCH_W, BOILER_SWITCH_H, TFT_BLACK);
    
    String modeText = "";
    switch (boilerMode) {
        case 0:
            modeText = "Auto";
            break;
        case 2:
            modeText = "2kW";
            break;
        case 4:
            modeText = "4kW";
            break;
        case 6:
            modeText = "6kW";
            break;
    }

    int textWidth = lcd.textWidth(modeText);
    int textX = BOILER_SWITCH_X + (BOILER_SWITCH_W - textWidth) / 2;
    int textY = BOILER_SWITCH_Y + (BOILER_SWITCH_H - lcd.fontHeight()) / 2;
    lcd.setTextSize(1);  // Textgröße festlegen
    lcd.setCursor(textX, textY);
    lcd.print(modeText);
}


void drawTibberPrice() {
  // Position und Größe des Rechtecks für den Preis
  int rectX = TIBBER_RECT_X;
  int rectY = TIBBER_RECT_Y;
  int rectW = ICON_WIDTH0;
  int rectH = ICON_WIDTH0;

  // Farbe für das Rechteck auf Dunkelgrün setzen
  uint16_t rectColor = TFT_DARKGREEN;  // Definiert als Dunkelgrün
  uint16_t textColor = TFT_WHITE;      // Textfarbe Weiß

  // Zeichne das Rechteck mit der gewünschten Farbe
  lcd.fillRect(rectX, rectY, rectW, rectH, rectColor);
  lcd.drawRect(rectX, rectY, rectW, rectH, TFT_BLACK);  // Optionaler schwarzer Rand

  // Setze die Textfarbe auf Weiß
  lcd.setTextColor(textColor);
  lcd.setTextSize(1);  // Textgröße festlegen
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Schriftart

  // Prüfen, ob ein gültiger Strompreis vorliegt
  if (currentElectricityPrice > 0) {
    // Strompreis in Cent umwandeln (EUR * 100)
    int priceInCents = int(currentElectricityPrice * 100);
    String priceText = String(priceInCents) + " c";  // Preis mit Cent-Symbol

    // Berechne die Position des Textes innerhalb des Rechtecks (zentriert)
    int textWidth = lcd.textWidth(priceText);
    int textX = rectX + (rectW - textWidth) / 2;
    int textY = rectY + (rectH - lcd.fontHeight()) / 2;

    // Setze den Cursor auf die Position und zeige den Preis an
    lcd.setCursor(textX, textY);
    lcd.print(priceText);
  } else {
    // Falls kein gültiger Preis vorliegt, "N/A" anzeigen
    String noPriceText = "N/A";
    int textWidth = lcd.textWidth(noPriceText);
    int textX = rectX + (rectW - textWidth) / 2;
    int textY = rectY + (rectH - lcd.fontHeight()) / 2;

    lcd.setCursor(textX, textY);
    lcd.print(noPriceText);
  }
}

void drawTibberPriceGraph(float tibberPrices[], int size) {
    // Diagrammbereich definieren
    int graphX = 50; // Rand links
    int graphY = 50; // Rand oben
    int graphHeight = lcd.height() * 2 / 3; // Höhe des Diagramms

    // Säulenbreite und Diagrammbreite berechnen, inklusive Lücken
    int totalGapWidth = size - 1; // Anzahl der Lücken zwischen den Säulen
    int availableWidth = lcd.width() - 130; // Verfügbare Breite für das Diagramm
    int barWidth = (availableWidth - totalGapWidth) / size; // Säulenbreite
    int graphWidth = (barWidth * size) + totalGapWidth; // Gesamte Diagrammbreite mit Lücken

    // Linken Bereich für "Heute" mit weißem Hintergrund zeichnen
    lcd.fillRect(graphX, graphY, graphWidth / 2, graphHeight, TFT_WHITE);

    // Rechten Bereich für "Morgen" mit blass-hellrosanem Hintergrund zeichnen
    uint16_t palePink = lcd.color565(255, 228, 240); // Blass hellrosa
    lcd.fillRect(graphX + graphWidth / 2, graphY, graphWidth / 2, graphHeight, palePink);

    // Rahmen des Diagramms zeichnen
    lcd.drawRect(graphX, graphY, graphWidth, graphHeight, TFT_BLACK);

    // Preisbereich ermitteln (nur die ersten 24 Stunden berücksichtigen)
    float minPrice = tibberPrices[0] * 100;
    float maxPrice = tibberPrices[0] * 100;
    for (int i = 1; i < 24; i++) { // Nur die ersten 24 Stunden betrachten
        float priceInCents = tibberPrices[i] * 100;
        if (priceInCents < minPrice) minPrice = priceInCents;
        if (priceInCents > maxPrice) maxPrice = priceInCents;
    }

    // Puffer für etwas höheren Maximalwert
    maxPrice += 3;
    minPrice -= 3;

    // Achsenbeschriftungen
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_BLACK);
    lcd.setCursor(graphX, graphY - 25);
    lcd.print("Tibber Preis (Cent/kWh)");

    // Wochentage auf Deutsch
    const char* germanWeekdays[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};

    // X-Achsenbeschriftung: Datum von heute und morgen
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Heute
        char todayLabel[16];
        snprintf(todayLabel, sizeof(todayLabel), "%s, %02d.%02d",
                 germanWeekdays[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1);

        // Morgen
        timeinfo.tm_mday += 1;
        mktime(&timeinfo); // Datum normalisieren
        char tomorrowLabel[16];
        snprintf(tomorrowLabel, sizeof(tomorrowLabel), "%s, %02d.%02d",
                 germanWeekdays[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1);

        // Beschriftung zeichnen
        lcd.setCursor(graphX + 20, graphY + graphHeight + 5);
        lcd.print(todayLabel);
        lcd.setCursor(graphX + graphWidth / 2 + 20, graphY + graphHeight + 5);
        lcd.print(tomorrowLabel);
    }

    // Preisskalierung
    float priceRange = maxPrice - minPrice;

    // Y-Achse mit Werten beschriften und horizontale Ticks zeichnen
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_BLACK);
    for (int i = 0; i <= 5; i++) { // 5 Ticks auf der Y-Achse
        float tickPrice = minPrice + i * (priceRange / 5.0);
        int tickY = graphY + graphHeight - (i * graphHeight / 5);

        // Horizontale Linie für jeden Tick zeichnen
        lcd.drawLine(graphX, tickY, graphX + graphWidth, tickY, TFT_LIGHTGREY);

        // Y-Achsenbeschriftung
        lcd.setCursor(graphX - 30, tickY - 5);
        lcd.printf("%.0f", tickPrice); // Preis in Cent anzeigen
    }

    // Vertikale Linien für jeden 6. Tick auf der X-Achse
    for (int i = 0; i < size; i++) {
        if (i % 6 == 0) { // Jeder 6. Tick
            int tickX = graphX + (i * (barWidth + 1)); // Inklusive Lücken
            lcd.drawLine(tickX, graphY, tickX, graphY + graphHeight, TFT_LIGHTGREY);
        }
    }

    // Säulen zeichnen
    for (int i = 0; i < size; i++) {
        int x = graphX + (i * (barWidth + 1)); // Berechnung der X-Position der Säule
        int barHeight = ((tibberPrices[i] * 100 - minPrice) / priceRange) * graphHeight;
        int y = graphY + graphHeight - barHeight; // Y-Position berechnen

        // Säule zeichnen
        uint16_t barColor = (i == getCurrentHour()) ? TFT_RED : TFT_BLUE; // Aktuelle Stunde in Rot, sonst Blau
        lcd.fillRect(x, y, barWidth, barHeight, barColor);
    }

    // Text mit Zeit und Preis für die aktuelle Stunde
    int currentHour = getCurrentHour(); // Aktuelle Stunde
    if (currentHour >= 0 && currentHour < size) {
        int currentX = graphX + (currentHour * (barWidth + 1));
        int currentPriceCents = tibberPrices[currentHour] * 100;

        lcd.setTextColor(TFT_DARKGREEN);
        lcd.setCursor(currentX + 5, graphY + 5);
        lcd.printf("%dh:", currentHour); // Aktuelle Stunde
        lcd.setCursor(currentX + 55, graphY + 5);
        lcd.printf("%d cent", currentPriceCents); // Preis in Cent
    }
}








void drawBrightnessButton() {
  lcd.fillRect(BRIGHTNESS_RECT_X, BRIGHTNESS_RECT_Y, BRIGHTNESS_RECT_W, BRIGHTNESS_RECT_H, TFT_GREY);
  lcd.drawRect(BRIGHTNESS_RECT_X, BRIGHTNESS_RECT_Y, BRIGHTNESS_RECT_W, BRIGHTNESS_RECT_H, TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_BLACK);
  int16_t textWidth = lcd.textWidth("B");
  int16_t textHeight = lcd.fontHeight();
  int16_t textX = BRIGHTNESS_RECT_X + (BRIGHTNESS_RECT_W - textWidth) / 2;
  int16_t textY = BRIGHTNESS_RECT_Y + (BRIGHTNESS_RECT_H - textHeight) / 2;
  
  lcd.setCursor(textX, textY);
  lcd.print("B");
}

void drawSunIcon() {
    // Define the size of the container where the icon and text will be placed
    const int containerWidth = 102;
    const int containerHeight = 70;

    // Define the text size, and calculate positioning
    const int textXOffset = SUN_ICON_X + 22 ;  // Start 10 pixels to the right of the icon
    const int textYOffset = SUN_ICON_Y + 3*(containerHeight / 4) - (lcd.fontHeight() / 2);  // Center text vertically

    // Draw the yellow background and black border for the container
    lcd.fillRect(SUN_ICON_X, SUN_ICON_Y, containerWidth, containerHeight, TFT_YELLOW);
    lcd.drawRect(SUN_ICON_X, SUN_ICON_Y, containerWidth, containerHeight, TFT_BLACK);

    // Draw the sun icon (40x40) on the left, 15 pixels from the top to center vertically
    lcd.fillRect(SUN_ICON_X + 2, SUN_ICON_Y + 2, 40, 40, TFT_BLACK);  // Clear the background for the icon
    lcd.drawBitmap(SUN_ICON_X + 2, SUN_ICON_Y + 2, sunIcon, 40, 40, TFT_YELLOW);  // Draw the sun icon

    // Fetch the PV power (from previously computed variable totalPvPower)
    float totalPvPower = (dcPvPower + acPvPower[0] + acPvPower[1] + acPvPower[2]) / 1000.0;  // Convert to kW

    // Format the PV power as a string with 1 decimal place
    String pvPowerText = String(totalPvPower, 1) + " kW";

    // Set text color and font
    lcd.setTextColor(TFT_BLACK);
    lcd.setTextSize(1);  // Adjust the text size as needed
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Use the same font

    // Draw the PV power text to the right of the sun icon, centered vertically
    lcd.setCursor(textXOffset, textYOffset);
    lcd.print(pvPowerText);
}


void drawTabButtons() {
  lcd.fillRect(439, 1, TAB1_BUTTON_W, 319, TFT_WHITE);
  // Draw Tab 1 button
  lcd.fillRect(TAB1_BUTTON_X, TAB1_BUTTON_Y, TAB1_BUTTON_W, TAB1_BUTTON_H, TFT_GREY);
  lcd.drawRect(TAB1_BUTTON_X, TAB1_BUTTON_Y, TAB1_BUTTON_W, TAB1_BUTTON_H, TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_BLACK);
  int16_t textWidth1 = lcd.textWidth("1");
  int16_t textHeight1 = lcd.fontHeight();
  int16_t textX1 = TAB1_BUTTON_X + (TAB1_BUTTON_W - textWidth1) / 2;
  int16_t textY1 = TAB1_BUTTON_Y + (TAB1_BUTTON_H - textHeight1) / 2;

  lcd.setCursor(textX1, textY1);
  lcd.print("1");

  // Draw Tab 2 button
  lcd.fillRect(TAB2_BUTTON_X, TAB2_BUTTON_Y, TAB2_BUTTON_W, TAB2_BUTTON_H, TFT_GREY);
  lcd.drawRect(TAB2_BUTTON_X, TAB2_BUTTON_Y, TAB2_BUTTON_W, TAB2_BUTTON_H, TFT_BLACK);

  lcd.setTextColor(TFT_BLACK);
  int16_t textWidth2 = lcd.textWidth("2");
  int16_t textHeight2 = lcd.fontHeight();
  int16_t textX2 = TAB2_BUTTON_X + (TAB2_BUTTON_W - textWidth2) / 2;
  int16_t textY2 = TAB2_BUTTON_Y + (TAB2_BUTTON_H - textHeight2) / 2;

  lcd.setCursor(textX2, textY2);
  lcd.print("2");

  // Draw Tab 3 button
  lcd.fillRect(TAB3_BUTTON_X, TAB3_BUTTON_Y, TAB3_BUTTON_W, TAB3_BUTTON_H, TFT_GREY);
  lcd.drawRect(TAB3_BUTTON_X, TAB3_BUTTON_Y, TAB3_BUTTON_W, TAB3_BUTTON_H, TFT_BLACK);

  lcd.setTextColor(TFT_BLACK);
  int16_t textWidth3 = lcd.textWidth("3");
  int16_t textHeight3 = lcd.fontHeight();
  int16_t textX3 = TAB3_BUTTON_X + (TAB3_BUTTON_W - textWidth3) / 2;
  int16_t textY3 = TAB3_BUTTON_Y + (TAB3_BUTTON_H - textHeight3) / 2;

  lcd.setCursor(textX3, textY3);
  lcd.print("3");
}

void drawCarConnectionButton() {
  uint16_t iconColor = (chargerStatus != 0) ? TFT_GREEN : TFT_GREY;
  lcd.fillRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
  lcd.drawBitmap(CAR_ICON_X, CAR_ICON_Y, carIcon, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, iconColor);
  lcd.drawRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
  oldChargerStatus = chargerStatus;
}

void drawSOCThreshold() {
  uint16_t color;
  switch (rectangleState) {
    case GREEN:
      SOC_THRESHOLD = 80;
      color = TFT_GREEN;
      break;
    case YELLOW:
      SOC_THRESHOLD = 90;
      color = TFT_YELLOW;
      break;
    case RED:
      SOC_THRESHOLD = 100;
      color = TFT_RED;
      break;
  }
  
  lcd.fillRect(SOC_RECT_X, SOC_RECT_Y, SOC_RECT_SIZE, SOC_RECT_SIZE, color);
  lcd.drawRect(SOC_RECT_X, SOC_RECT_Y, SOC_RECT_SIZE, SOC_RECT_SIZE, TFT_BLACK);
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Schriftart festlegen
  int textWidth = lcd.textWidth(String((int)SOC_THRESHOLD) + "%");
  int textHeight = lcd.fontHeight();
  int textX = SOC_RECT_X + (SOC_RECT_SIZE - textWidth) / 2;
  int textY = SOC_RECT_Y + (SOC_RECT_SIZE - textHeight) / 2;
  lcd.fillRect(textX, textY, textWidth, textHeight, color);
  lcd.setTextColor(TFT_BLACK);
  lcd.setCursor(textX, textY);
  lcd.print(String((int)SOC_THRESHOLD) + "%");

  oldRectangleState = rectangleState;
}



void drawChargeModeButton() {
    // Wähle eine Hintergrundfarbe
    uint16_t backgroundColor = lcd.color565(173, 216, 230);
    uint16_t borderColor = TFT_BLACK;

    // Fülle den Button-Bereich mit der Hintergrundfarbe
    lcd.fillRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, backgroundColor);
    lcd.drawRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, borderColor);

    // Setze die Textfarbe
    lcd.setTextColor(TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Schriftart festlegen
    // Aktualisiere den Button-Text basierend auf dem aktuellen Lade-Modus
    String modeText;
    switch (chargeMode) {
        case 0:
            modeText = "Manuell";
            break;
        case 1:
            modeText = "Auto";
            break;
        case 2:
            modeText = "Zeitplan";
            break;
        default:
            modeText = "Unknown";
            break;
    }

    // Ladeleistung (chargePower) in kW umrechnen
    float powerKW = chargePower / 1000.0;  // Angenommen, chargePower ist in Watt

    // Erstelle den Text für die Ladeleistung
    String powerText = String(powerKW, 1) + " kW";

    // Berechnung der Position für die erste Zeile (Modus)
    
    int16_t modeTextWidth = lcd.textWidth(modeText);
    int16_t modeTextHeight = lcd.fontHeight();
    int16_t modeTextX = BUTTON_X + (BUTTON_W - modeTextWidth) / 2;
    int16_t modeTextY = BUTTON_Y + (BUTTON_H / 4) - (modeTextHeight / 2);  // Etwas weiter oben positionieren

    // Berechnung der Position für die zweite Zeile (Ladeleistung)
    int16_t powerTextWidth = lcd.textWidth(powerText);
    int16_t powerTextHeight = lcd.fontHeight();
    int16_t powerTextX = BUTTON_X + (BUTTON_W - powerTextWidth) / 2;
    int16_t powerTextY = BUTTON_Y + (3 * BUTTON_H / 4) - (powerTextHeight / 2);  // Etwas weiter unten positionieren

    // Setze die Cursorposition und drucke den Modus in der ersten Zeile
    lcd.setCursor(modeTextX, modeTextY);
    lcd.print(modeText);

    // Setze die Cursorposition und drucke die Ladeleistung in der zweiten Zeile
    lcd.setCursor(powerTextX, powerTextY);
    lcd.print(powerText);
}



void drawStartStopButton() {
  // Always redraw the button regardless of the state change
  uint16_t color = (startStopCharging == 1) ? TFT_GREEN : TFT_RED;
  String buttonText = (startStopCharging == 1) ? "An" : "Aus";
  lcd.fillRect(START_STOP_RECT_X, START_STOP_RECT_Y, START_STOP_RECT_SIZE, START_STOP_RECT_SIZE, color);
  lcd.drawRect(START_STOP_RECT_X, START_STOP_RECT_Y, START_STOP_RECT_SIZE, START_STOP_RECT_SIZE, TFT_BLACK);
  lcd.setTextColor(TFT_BLACK);
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Schriftart festlegen
  int16_t textWidth = lcd.textWidth(buttonText);
  int16_t textHeight = lcd.fontHeight();
  int16_t textX = START_STOP_RECT_X + (START_STOP_RECT_SIZE - textWidth) / 2;
  int16_t textY = START_STOP_RECT_Y + (START_STOP_RECT_SIZE - textHeight) / 2;
  lcd.setCursor(textX, textY);
  lcd.print(buttonText);
  oldStartStopCharging = startStopCharging;
}

void drawCarRangeButton() {
    // Konstanten für die Berechnung
    const float maxCapacity = 32.3;         // Maximale Kapazität des E-UP in kWh
    const float consumptionPer100Km = 13.5; // Durchschnittlicher Verbrauch in kWh/100 km

    // Berechnung der Reichweite basierend auf dem SOC-Wert
    float range = (socValue / 100.0) * (maxCapacity / consumptionPer100Km); // Reichweite in km

    // Button-Hintergrundfarbe und Textfarbe festlegen
    uint16_t backgroundColor = TFT_LIGHTGREY;  // Button-Hintergrund
    uint16_t borderColor = TFT_BLACK;          // Randfarbe
    uint16_t textColor = TFT_BLACK;            // Textfarbe

    // Button-Bereich zeichnen
    lcd.fillRect(CAR_RANGE_X, CAR_RANGE_Y, CAR_RANGE_WIDTH, CAR_RANGE_HEIGHT, backgroundColor);
    lcd.drawRect(CAR_RANGE_X, CAR_RANGE_Y, CAR_RANGE_WIDTH, CAR_RANGE_HEIGHT, borderColor);

    // Text (Reichweite) vorbereiten und formatieren
    String rangeText = String(range, 0) + " km";  // Reichweite mit einer Nachkommastelle und "km"
    lcd.setTextColor(textColor);
    lcd.setTextSize(1);  // Textgröße anpassen
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Schriftart festlegen

    // Textgröße berechnen und zentrieren
    int textWidth = lcd.textWidth(rangeText);
    int textHeight = lcd.fontHeight();
    int textX = CAR_RANGE_X + (CAR_RANGE_WIDTH - textWidth) / 2;
    int textY = CAR_RANGE_Y + (CAR_RANGE_HEIGHT - textHeight) / 2;

    // Text anzeigen
    lcd.setCursor(textX, textY);
    lcd.print(rangeText);
}


void drawManualModePhaseButton() {
  // Choose a background color based on the current phase state
  uint16_t backgroundColor = (manualModePhase == 0) ? TFT_GREEN : TFT_YELLOW;

  // Draw the button with the corresponding color
  lcd.fillRect(MANUAL_MODE_PHASE_RECT_X, MANUAL_MODE_PHASE_RECT_Y, MANUAL_MODE_PHASE_RECT_W, MANUAL_MODE_PHASE_RECT_H, backgroundColor);
  lcd.drawRect(MANUAL_MODE_PHASE_RECT_X, MANUAL_MODE_PHASE_RECT_Y, MANUAL_MODE_PHASE_RECT_W, MANUAL_MODE_PHASE_RECT_H, TFT_BLACK);

  // Choose the label based on the current phase
  String phaseText = (manualModePhase == 0) ? "2 P" : "1 P";  // 2 phases at 0, 1 phase at 1

  // Center and display the text
  int16_t textWidth = lcd.textWidth(phaseText);
  int16_t textHeight = lcd.fontHeight();
  int16_t textX = MANUAL_MODE_PHASE_RECT_X + (MANUAL_MODE_PHASE_RECT_W - textWidth) / 2;
  int16_t textY = MANUAL_MODE_PHASE_RECT_Y + (MANUAL_MODE_PHASE_RECT_H - textHeight) / 2;

  lcd.setCursor(textX, textY);
  lcd.print(phaseText);
}

void drawStaticText() {
  lcd.setTextColor(TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

  lcd.setCursor(BORDER, 400);
  lcd.print("Zeit:");
}




bool isWithinButton(int32_t touchX, int32_t touchY, int32_t btnX, int32_t btnY, int32_t btnW, int32_t btnH) {
    return (touchX >= btnX && touchX <= (btnX + btnW) &&
            touchY >= btnY && touchY <= (btnY + btnH));
}

void queueModbusWrite(IPAddress server, int reg, uint16_t value, uint8_t unitID = MODBUSIP_UNIT) {
  ModbusWriteRequest request = {server, reg, value, unitID};
  xQueueSend(modbusWriteQueue, &request, portMAX_DELAY);
}

void toggleManualModePhase() {
  // Toggle between phases (assuming there are 2 phases)
  manualModePhase = (manualModePhase + 1) % 2;
  
  // Redraw the button with the updated state
  drawManualModePhaseButton();

  // Send the new value over Modbus
  queueModbusWrite(remoteEVCS, MANUAL_MODE_PHASE_REG, manualModePhase);
}

void toggleChargeMode() {
    // Increment chargeMode and wrap it around if it exceeds 2
    chargeMode = (chargeMode + 1) % 3;

    // DEBUG: Print the new charge mode
    Serial.printf("New ChargeMode: %d\n", chargeMode);

    // Update the display immediately with the new mode
    drawChargeModeButton();

    // Queue the Modbus write request after the display is updated
    queueModbusWrite(remoteEVCS, CHARGE_MODE_REG, chargeMode);
}

void drawCarIconWithSOC() {
  // Verwende den globalen chargerStatus, der durch die Modbus-Abfrage aktualisiert wurde
  uint16_t iconColor = (chargerStatus != 0) ? TFT_GREEN : lcd.color565(173, 216, 230);  // Grün, wenn Auto angeschlossen, sonst die gewünschte Farbe
  lcd.fillRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
  lcd.drawBitmap(CAR_ICON_X, CAR_ICON_Y, carIcon, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, iconColor);
  lcd.drawRect(CAR_ICON_X, CAR_ICON_Y, CAR_ICON_WIDTH, CAR_ICON_HEIGHT, TFT_BLACK);
  // SOC mit Prozentzeichen und ohne Nachkommastelle anzeigen
  int socPercentage = socValue / 100;  // SOC als Ganzzahlwert (ohne Nachkommastellen)
  // Hintergrundfarbe für das SOC-Rechteck (gleich wie Icon, wenn Auto nicht angeschlossen)
  uint16_t socBackgroundColor = iconColor;  // Verwende dieselbe Farbe wie für das Icon
  // Zeichne das Rechteck für den SOC-Wert
  int rectWidth = 50;  // Breite des Rechtecks
  int rectHeight = 30;  // Höhe des Rechtecks
  int rectX = CAR_ICON_X + 36;  // X-Position des Rechtecks
  int rectY = CAR_ICON_Y + 22;  // Y-Position des Rechtecks

  lcd.fillRect(rectX, rectY, rectWidth, rectHeight, socBackgroundColor);  // Zeichne das Rechteck mit entsprechender Hintergrundfarbe

  // Textfarbe und Schriftart setzen
  lcd.setTextColor(TFT_BLACK);  // Textfarbe
  lcd.setTextSize(1);  // Textgröße anpassen
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Schriftart setzen

  // String mit SOC und Prozentzeichen erstellen
  String socText = String(socPercentage) + "%";

  // Text zentrieren
  int textWidth = lcd.textWidth(socText);
  int textHeight = lcd.fontHeight();
  int textX = rectX + (rectWidth - textWidth) / 2;  // X-Position zentrieren
  int textY = rectY + (rectHeight - textHeight) / 2;  // Y-Position zentrieren

  // SOC-Wert mit Prozentzeichen anzeigen
  lcd.setCursor(textX, textY);
  lcd.print(socText);  // SOC mit Prozentzeichen anzeigen
}




void toggleStartStopCharging() {
  startStopCharging = (startStopCharging == 1) ? 0 : 1;
  queueModbusWrite(remoteEVCS, START_STOP_CHARGING_REG, startStopCharging);
  drawStartStopButton();
}

void toggleBrightness() {
  brightnessLevel = (brightnessLevel == 255) ? 10 : 255; // Toggle between full brightness and night mode
  adjustBrightness(brightnessLevel);
  //drawBrightnessButton(); // Update button display
}







void switchTab(int tab) {
      currentTab = tab;
    lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
    
    drawTabButtons();            // Draw tab buttons
    drawBrightnessButton();      // Draw brightness button

    // Reset font to ensure consistency across tabs
    lcd.setTextColor(TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    // Redraw all elements for the selected tab
    if (currentTab == 1) {
        displayData();            // Redraw all dynamic data
        drawStaticText();         // Draw static text
    } else if (currentTab == 2) {
        drawClockTab();           // Draw clock on tab 2
        drawBoilerSwitch();
    } else if (currentTab == 3) {
        displayData();            // Display data on tab 3
    }
}

void drawWaterTempButton() {
  // Zeichne das Rechteck für die Wassertemperatur
  lcd.fillRect(H2O_RECT_X, H2O_RECT_Y, H2O_RECT_SIZE, H2O_RECT_SIZE, TFT_BLUE);  // Blaues Rechteck als Hintergrund
  lcd.drawRect(H2O_RECT_X, H2O_RECT_Y, H2O_RECT_SIZE, H2O_RECT_SIZE, TFT_BLACK);  // Schwarzer Rand

  // Wassertemperatur anzeigen (Annahme: Temperatur in Hundertsteln eines Grads)
  float waterTempCelsius = waterTemperature / 100;  // Wandeln in Grad Celsius

  // Textfarbe und -größe einstellen
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(1);
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);  // Groß genug für die Anzeige
  //lcd.setFont(&lgfx::v1::fonts::FreeSans9pt7b); 
    // Temperatur ohne Gradzeichen anzeigen
  String tempText = String(waterTempCelsius, 0);  // Temperatur mit einer Nachkommastelle
  int16_t textWidth = lcd.textWidth(tempText);
  int16_t textHeight = lcd.fontHeight();
  int16_t textX = H2O_RECT_X + (H2O_RECT_SIZE - textWidth - 26) / 2;  // Platz für das Gradzeichen lassen
  int16_t textY = H2O_RECT_Y + (H2O_RECT_SIZE - textHeight) / 2;

  // Temperatur anzeigen
  lcd.setCursor(textX, textY);
  lcd.print(tempText);

  // Gradzeichen als Kreis zeichnen
  int16_t circleX = textX + textWidth + 8;  // X-Position des Kreises (leicht versetzt nach rechts)
  int16_t circleY = textY - 1;               // Y-Position des Kreises (leicht über der Zahl)
  int16_t radius = 3;                        // Radius des Kreises

  lcd.drawCircle(circleX, circleY, radius, TFT_WHITE);  // Kreis als Gradzeichen

  // "C" für Celsius hinter das Gradzeichen setzen
  lcd.setCursor(circleX + 4, textY);
  lcd.print("C");
}


void drawHouseIcon() {
    uint16_t iconColor = TFT_GREEN;  // Sie können die Farbe nach Belieben anpassen
    lcd.fillRect(HOUSE_ICON_X, HOUSE_ICON_Y, HOUSE_ICON_WIDTH, HOUSE_ICON_HEIGHT, TFT_BLACK);
    lcd.drawBitmap(HOUSE_ICON_X, HOUSE_ICON_Y, houseIcon, HOUSE_ICON_WIDTH, HOUSE_ICON_HEIGHT, iconColor);
    lcd.fillRect(HOUSE_ICON_X+HOUSE_ICON_WIDTH, HOUSE_ICON_Y, 80, HOUSE_ICON_HEIGHT, TFT_GREEN);
    lcd.drawRect(HOUSE_ICON_X, HOUSE_ICON_Y, HOUSE_ICON_WIDTH+80, HOUSE_ICON_HEIGHT, TFT_BLACK);
}

void drawClockTab() {
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
        char timeStr[10];
        strftime(timeStr, sizeof(timeStr), "%H:%M Uhr", &timeinfo);
        
        lcd.setTextColor(TFT_BLACK);
        lcd.setTextSize(2);
        int16_t textWidth = lcd.textWidth(timeStr);
        int16_t textX = (TAB1_BUTTON_X - textWidth) / 2;
        int16_t textY = (TFT_HEIGHT - lcd.fontHeight()) / 2;

        lcd.setCursor(textX, textY);
        lcd.print(timeStr);
    } else {
        // If the time could not be retrieved, display an error message
        lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
        lcd.setTextColor(TFT_RED);
        lcd.setTextSize(1);
        int16_t errorTextX = (TAB1_BUTTON_X - lcd.textWidth("Time Error")) / 2;
        int16_t errorTextY = (TFT_HEIGHT - lcd.fontHeight()) / 2;

        lcd.setCursor(errorTextX, errorTextY);
        lcd.print("Time Error");
    }
}

void displayTimestamp(uint16_t timestampHigh, uint16_t timestampLow, int x, int y) {
    // Kombiniere die beiden Teile zu einem einzigen 32-Bit-Timestamp
    uint32_t timestamp = ((uint32_t)timestampHigh << 16) | timestampLow;

    // Überprüfe, ob der Zeitstempel gültig ist
    String timestampText;
    if (timestamp > 0) {
        // Wandle den Zeitstempel in eine lesbare Zeit um
        time_t rawtime = timestamp;
        struct tm *timeinfo = localtime(&rawtime);

        // Erstelle das Zeitformat: "dd.mm um HH:MM Uhr"
        char buffer[30];
        strftime(buffer, 30, "%d.%m um %H:%M Uhr", timeinfo);
        timestampText = String(buffer);
    } else {
        // Meldung, falls der Zeitstempel ungültig ist
        timestampText = "Ungültiger Zeitstempel";
    }

    // Lösche den Bereich und zeige den Zeitstempel an der angegebenen Position an
    lcd.fillRect(x, y, 250, 20, TFT_WHITE);
    lcd.setCursor(x, y);
    lcd.print(timestampText);
}


void displayData() {
    if (currentTab == 1) {
        
        //displayTimestamp(timestampHigh, timestampLow, BORDER, BORDER + 130 + 2 * ICON_HEIGHT);
        // SOC-basierte Reichweite anzeigen
        drawCarRangeButton();
        // Redraw other UI elements
        drawSOCThreshold();
        drawChargeModeButton();
        drawStartStopButton();
        drawCarConnectionButton();
        drawCarIconWithSOC();  // SOC über dem Auto anzeigen
        drawManualModePhaseButton();
        drawWaterTempButton();
        drawSunIcon();
        drawTibberPrice();
        drawHouseIcon();
        drawPylontechSOCWithPower();  
        drawGridPowerButton(); 
    } else if (currentTab == 2) {
        lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
        //drawBoilerSwitch();
        //drawClockTab(); // Update the clock on tab 2
        
    } else if (currentTab == 3) {
        lcd.fillRect(0, 0, TAB1_BUTTON_X - 1, TFT_HEIGHT, TFT_WHITE);
        // Display Pylontech SOC on Tab 3
         drawTibberPriceGraph(tibberPrices, 48);
        
    
    }
}


void drawGridPowerButton() {
    // Zeichne das GRID-Icon und den Hintergrund
    //uint16_t iconColor = TFT_BLUE;
    lcd.fillRect(GRID_ICON_X, GRID_ICON_Y, GRID_ICON_WIDTH, GRID_ICON_HEIGHT, TFT_BLUE);
    //lcd.drawBitmap(GRID_ICON_X, GRID_ICON_Y, sunIcon, GRID_ICON_WIDTH, GRID_ICON_HEIGHT, iconColor);
    lcd.drawRect(GRID_ICON_X, GRID_ICON_Y, GRID_ICON_WIDTH, GRID_ICON_HEIGHT, TFT_BLACK);

    // Text für die Summenleistung formatieren
    String gridPowerText = String(totalGridPowerKW, 2) + " kW";

    // Textgröße und -position berechnen
    lcd.setTextColor(TFT_WHITE);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
    int textWidth = lcd.textWidth(gridPowerText);
    int textHeight = lcd.fontHeight();
    int textX = GRID_ICON_X + (GRID_ICON_WIDTH - textWidth) / 2;
    int textY = GRID_ICON_Y + (GRID_ICON_HEIGHT - textHeight) / 2;
    // Summenleistung anzeigen
    lcd.setCursor(textX, textY);
    lcd.print(gridPowerText);
}


void drawPylontechSOCWithPower() {
    // SOC der Batterie
    float pylsoc = PylontechSOC;  // Pylontech SOC in Prozent
    char SOCStr[10];
    snprintf(SOCStr, sizeof(SOCStr), "%.0f %%", pylsoc);

    // Batterieleistung: Umwandlung in kW, Berücksichtigung negativer Werte
    int16_t signedBatteryPower = (int16_t)batteryPower;  // Konvertiere zu signed, falls negative Werte erwartet werden
    float batteryPowerKW = signedBatteryPower / 1000.0;  // batteryPower ist in Watt und wird in kW umgerechnet

    // Text für die Batterieleistung, einschließlich negativer Werte für Entladung
    char powerStr[20];
    snprintf(powerStr, sizeof(powerStr), "%.1fkW", batteryPowerKW);  // Leistung in kW mit einer Nachkommastelle

    // Koordinaten für das Rechteck
    int16_t rectX = HOUSE_ICON_X + HOUSE_ICON_WIDTH;
    int16_t rectY = HOUSE_ICON_Y;
    int16_t rectWidth = 80;
    int16_t rectHeight = HOUSE_ICON_HEIGHT;

    // SOC-Anzeige zentrieren
    lcd.setTextColor(TFT_BLACK);
    lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);

    // Berechnung der Position für die erste Zeile (SOC)
    int16_t socTextWidth = lcd.textWidth(SOCStr);
    int16_t socTextHeight = lcd.fontHeight();
    int16_t socTextX = rectX + (rectWidth - socTextWidth) / 2;
    int16_t socTextY = rectY + (rectHeight / 4) - (socTextHeight / 2);  // Etwas weiter oben positionieren

    // Berechnung der Position für die zweite Zeile (Leistung)
    int16_t powerTextWidth = lcd.textWidth(powerStr);
    int16_t powerTextHeight = lcd.fontHeight();
    int16_t powerTextX = rectX + (rectWidth - powerTextWidth) / 2;
    int16_t powerTextY = rectY + (3 * rectHeight / 4) - (powerTextHeight / 2);  // Etwas weiter unten positionieren

    // Zeichnen der SOC-Anzeige
    lcd.setCursor(socTextX, socTextY);
    lcd.print(SOCStr);

    // Zeichnen der Leistungsanzeige
    lcd.setCursor(powerTextX, powerTextY);
    lcd.print(powerStr);
}


bool connectModbusServer(IPAddress server, int maxRetries = 2) {
    bool connected = false;
    int retryCount = 0;

    while (!connected && retryCount < maxRetries) {
        Serial.printf("Attempting to connect to Modbus server: %s (Attempt %d)\n", server.toString().c_str(), retryCount + 1);
        connected = mb.connect(server);
        if (connected) {
            Serial.println("Successfully connected to Modbus server.");
            return true;
        } else {
            Serial.println("Connection failed. Waiting before retry...");
            retryCount++;
            delay(4000);  // Wait 4 seconds before retrying
        }
    }

    if (!connected) {
        Serial.println("Failed to connect to Modbus server after multiple attempts.");
    }

    return connected;
}

void modbusTask(void *parameter) {
    const int longInterval = 20000;  // Interval for normal queries (5 seconds)
    const int shortInterval = 2000;  // Interval for immediate queries (2 seconds)

    while (true) {
        // Reconnect to EVCS server if the connection is lost
        if (!evcsConnected) {
            evcsConnected = connectModbusServer(remoteEVCS, 5);  // Retry up to 5 times
        }

        if (evcsConnected) {
            // Perform Modbus read only if connected
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteEVCS, MANUAL_MODE_PHASE_REG, manualModePhase);
            readModbusData(remoteEVCS, CHARGE_MODE_REG, chargeMode);  // Read the charge mode
            readModbusData(remoteEVCS, CHARGE_POWER_REG, chargePower);  // Read the charge power
            readModbusData(remoteEVCS, START_STOP_CHARGING_REG, startStopCharging);  // Read the start/stop charging status
            readModbusData(remoteEVCS, CHARGER_STATUS_REG, chargerStatus);  // Read charger status
            xSemaphoreGive(modbusMutex);
        }

        // Reconnect to SOC server if the connection is lost
        if (!socConnected) {
            socConnected = connectModbusServer(remoteSOC, 5);  // Retry up to 5 times
        }

        if (socConnected) {
            // Perform Modbus read only if connected
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteSOC, SOC_REG, socValue);
            readModbusData(remoteSOC, TIMESTAMP_HIGH_REG, timestampHigh);
            readModbusData(remoteSOC, TIMESTAMP_LOW_REG, timestampLow);
            xSemaphoreGive(modbusMutex);

            // Handle SOC threshold logic
            if (socValue > SOC_THRESHOLD * 100) {  // Check if SOC exceeds the threshold
                if (startStopCharging == 1) {  // If charging is active
                    Serial.println("SOC over threshold. Stopping charging...");
                    startStopCharging = 0;  // Stop charging
                    queueModbusWrite(remoteEVCS, START_STOP_CHARGING_REG, startStopCharging);
                    drawStartStopButton();  // Update the display
                }
            }
        }

        // Reconnect to CERBO GX if the connection is lost
        if (!cerboConnected) {
            cerboConnected = connectModbusServer(remoteCERBO, 5);  // Retry up to 5 times
        }

        if (cerboConnected) {
            // Perform Modbus read only if connected
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteCERBO, PYLONTECH_SOC_REG, PylontechSOC, CERBO_UNIT_ID);  // Unit ID added
            readModbusData(remoteCERBO, WATER_TEMP_REG, waterTemperature, CERBO_UNIT_ID_TEMP);  // Read water temperature
            readModbusData(remoteCERBO, 842, batteryPower, CERBO_UNIT_ID);  // Read Battery Power (Watt)
            readModbusData(remoteCERBO, DC_PV_POWER_REG, dcPvPower, CERBO_UNIT_ID);
            for (int i = 0; i < 3; i++) {// Read AC-coupled PV power phases (registers 811, 812, 813)
              readModbusData(remoteCERBO, AC_PV_POWER_REGS[i], acPvPower[i], CERBO_UNIT_ID);
            }
            readModbusData(remoteCERBO, GRID_PHASE1_REG, rawgridPhase1, CERBO_UNIT_ID);
            readModbusData(remoteCERBO, GRID_PHASE2_REG, rawgridPhase2, CERBO_UNIT_ID);
            readModbusData(remoteCERBO, GRID_PHASE3_REG, rawgridPhase3, CERBO_UNIT_ID);  
            // Berechne die Summenleistung der GRID-Phasen in kW
            int16_t gridPhase1 = static_cast<int16_t>(rawgridPhase1);
            int16_t gridPhase2 = static_cast<int16_t>(rawgridPhase2);
            int16_t gridPhase3 = static_cast<int16_t>(rawgridPhase3);
            totalGridPowerKW = (gridPhase1 + gridPhase2 + gridPhase3) / 1000.0;
            
            xSemaphoreGive(modbusMutex);
            
         }
            
        // Display data on the screen
        displayData();
        
        if (immediateModbusRequest) {
            // Handle immediate Modbus request if needed
            immediateModbusRequest = false;
        }

        // Wait between queries
        vTaskDelay(immediateModbusRequest ? shortInterval / portTICK_PERIOD_MS : longInterval / portTICK_PERIOD_MS);
    }
}

void modbusWriteTask(void *parameter) {
    ModbusWriteRequest request;

    while (true) {
        if (xQueueReceive(modbusWriteQueue, &request, portMAX_DELAY)) {
            // Take mutex for thread safety
            xSemaphoreTake(modbusMutex, portMAX_DELAY);

            // Attempt to write the Modbus data
            bool success = writeModbusData(request.server, request.reg, request.value, request.unitID);

            // Release the mutex
            xSemaphoreGive(modbusMutex);

            // Log the success or failure
            if (success) {
                Serial.println("Modbus write completed successfully.");
            } else {
                Serial.println("Modbus write failed.");
            }
        }
    }
}

void touchTask(void *parameter) {
    int32_t x, y;

    while (true) {
        if (lcd.getTouch(&x, &y)) {
            lastInteractionTime = millis();
            Serial.printf("Touch detected at (%d, %d). Last interaction time updated to %lu\n", x, y, lastInteractionTime);
            turnOnDisplay();
            // Handle touch logic only if the display is on
            if (displayOn) {
                unsigned long currentTime = millis();
                if (currentTime - lastTouchTime > debounceDelay) {
                    lastTouchTime = currentTime;
                    Serial.printf("Handling touch after debounce. Current time: %lu, Last touch time: %lu\n", currentTime, lastTouchTime);

                    immediateModbusRequest = true;  // Trigger immediate Modbus request
                    Serial.println("Immediate Modbus request triggered.");

                    if (currentTab == 1) {
                        // Charge Mode Button
                        if (isWithinButton(x, y, BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
                            Serial.println("Charge Mode Button pressed.");
                            toggleChargeMode();
                        }
                        // Start/Stop Charging Button
                        else if (isWithinButton(x, y, START_STOP_RECT_X, START_STOP_RECT_Y, START_STOP_RECT_SIZE, START_STOP_RECT_SIZE)) {
                            Serial.println("Start/Stop Charging Button pressed.");
                            toggleStartStopCharging();
                        }
                        // SOC Threshold Button
                        else if (isWithinButton(x, y, SOC_RECT_X, SOC_RECT_Y, SOC_RECT_SIZE, SOC_RECT_SIZE)) {
                            Serial.println("SOC Threshold Button pressed.");
                            rectangleState = (rectangleState == GREEN) ? YELLOW : (rectangleState == YELLOW) ? RED : GREEN;
                            drawSOCThreshold();
                        }
                        // Manual Mode Phase Button
                        else if (isWithinButton(x, y, MANUAL_MODE_PHASE_RECT_X, MANUAL_MODE_PHASE_RECT_Y, MANUAL_MODE_PHASE_RECT_W, MANUAL_MODE_PHASE_RECT_H)) {
                            Serial.println("Manual Mode Phase Button pressed.");
                            toggleManualModePhase();  // Call function for the new button
                        }
                        // Tibber Button -> Wechselt zu Tab 3
                        else if (isWithinButton(x, y, TIBBER_RECT_X, TIBBER_RECT_Y, TIBBER_RECT_SIZE, TIBBER_RECT_SIZE)) {
                            Serial.println("Tibber Button pressed. Switching to Tab 3.");
                            switchTab(3);
                        }

                    }

                    if (currentTab == 2) {
                        // Boiler Switch Button
                        if (isWithinButton(x, y, BOILER_SWITCH_X, BOILER_SWITCH_Y, BOILER_SWITCH_W, BOILER_SWITCH_H)) {
                            Serial.println("Boiler Switch Button pressed.");
                            toggleBoilerMode();  // Switch between Auto, 2kW, 4kW, and 6kW
                            drawBoilerSwitch();  // Update the display after changing the mode
                        }
                    }

                    // Brightness Control Button (applies to all tabs)
                    if (isWithinButton(x, y, BRIGHTNESS_RECT_X, BRIGHTNESS_RECT_Y, BRIGHTNESS_RECT_W, BRIGHTNESS_RECT_H)) {
                        Serial.println("Brightness Control Button pressed.");
                        toggleBrightness();
                    }
                    // Tab 1 Button
                    else if (isWithinButton(x, y, TAB1_BUTTON_X, TAB1_BUTTON_Y, TAB1_BUTTON_W, TAB1_BUTTON_H)) {
                        Serial.println("Tab 1 Button pressed.");
                        switchTab(1);
                    }
                    // Tab 2 Button
                    else if (isWithinButton(x, y, TAB2_BUTTON_X, TAB2_BUTTON_Y, TAB2_BUTTON_W, TAB2_BUTTON_H)) {
                        Serial.println("Tab 2 Button pressed.");
                        switchTab(2);
                    }
                    // Tab 3 Button
                    else if (isWithinButton(x, y, TAB3_BUTTON_X, TAB3_BUTTON_Y, TAB3_BUTTON_W, TAB3_BUTTON_H)) {
                        Serial.println("Tab 3 Button pressed.");
                        switchTab(3);  // Switch to Tab 3
                    }
                }
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}


void turnOffDisplay() {
  if (displayOn) {
    lcd.setBrightness(0);  // Set brightness to 0 to effectively turn off the display
    displayOn = false;
    immediateModbusRequest = false;
    Serial.println("Display turned off due to inactivity.");
  }
}

void turnOnDisplay() {
  if (!displayOn) {
    Serial.println("Turning on display...");
    lcd.init();
    adjustBrightness(brightnessLevel);  // Restore brightness to previous level
    switchTab(1);  // Display Tab 1 again
    displayOn = true;
    immediateModbusRequest = true;
    Serial.println("Display turned on by touch.");
  }
}

void adjustBrightness(int level) {
  brightnessLevel = constrain(level, 0, 255);
  lcd.setBrightness(brightnessLevel);
  Serial.printf("Brightness adjusted to %d\n", brightnessLevel);
}

void autoAdjustBrightness() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  int hour = timeinfo.tm_hour;
  if (hour >= 20 || hour < 6) {
    adjustBrightness(10);  // Night Mode
  } else {
    adjustBrightness(255); // Day Mode
  }
}

void connectToWiFi() {
  IPAddress local_IP(192, 168, 178, 155); // Festgelegte IP-Adresse
  IPAddress gateway(192, 168, 178, 1);    // Standard-Gateway (hier angepasst, falls notwendig)
  IPAddress subnet(255, 255, 255, 0);     // Subnetzmaske
  IPAddress primaryDNS(8, 8, 8, 8);       // DNS-Server (hier Google DNS, optional)
  IPAddress secondaryDNS(8, 8, 4, 4);     // Sekundärer DNS-Server (optional)

  // Setze die feste IP-Konfiguration
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA-Failed to configure!");
  }

  int attemptCount = 0;
  bool connected = false;
  
  Serial.print("Connecting to primary WiFi...");
  WiFi.begin(primarySSID, primaryPassword);
  
  while (WiFi.status() != WL_CONNECTED && attemptCount < 20) {
    delay(500);
    Serial.print(".");
    attemptCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to primary WiFi!");
    connected = true;
  } else {
    Serial.println("\nPrimary WiFi connection failed, trying secondary WiFi...");
    WiFi.begin(secondarySSID, secondaryPassword);
    attemptCount = 0;

    while (WiFi.status() != WL_CONNECTED && attemptCount < 20) {
      delay(500);
      Serial.print(".");
      attemptCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to secondary WiFi!");
      connected = true;
    }
  }

  if (!connected) {
    Serial.println("\nFailed to connect to any WiFi network.");
    lcd.fillRect(0, 32, TFT_WIDTH, 30, TFT_RED);
    lcd.setCursor(0, 32);
    lcd.setTextColor(TFT_WHITE);
    lcd.print("WiFi connection failed!");
  }
}


void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Reconnecting...");
        connectToWiFi();  // Attempt to reconnect
    }
  
    // WLAN-Symbol basierend auf dem Verbindungsstatus aktualisieren
    bool connected = (WiFi.status() == WL_CONNECTED);
    drawWiFiIcon(connected);

    // Falls eine neue Verbindung zum WLAN hergestellt wurde, prüfe und erneuere die SOC-Server-Verbindung
    if (WiFi.status() == WL_CONNECTED && (!socConnected || !evcsConnected || !cerboConnected)) {
        Serial.println("WiFi reconnected. Reinitializing Modbus connections...");

        // Reinitialisiere alle Modbusverbindungen
        socConnected = connectModbusServer(remoteSOC, 5);
        evcsConnected = connectModbusServer(remoteEVCS, 5);
        cerboConnected = connectModbusServer(remoteCERBO, 5);

        // Nach Reconnect: Daten erneut abfragen, um sicherzustellen, dass die neuesten Werte angezeigt werden
        if (socConnected) {
            Serial.println("Re-fetching SOC data...");
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteSOC, SOC_REG, socValue);
            readModbusData(remoteSOC, TIMESTAMP_HIGH_REG, timestampHigh);
            readModbusData(remoteSOC, TIMESTAMP_LOW_REG, timestampLow);
            xSemaphoreGive(modbusMutex);
        }

        if (evcsConnected) {
            Serial.println("Re-fetching EVCS data...");
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteEVCS, MANUAL_MODE_PHASE_REG, manualModePhase);
            readModbusData(remoteEVCS, CHARGE_MODE_REG, chargeMode);
            readModbusData(remoteEVCS, CHARGE_POWER_REG, chargePower);
            readModbusData(remoteEVCS, START_STOP_CHARGING_REG, startStopCharging);
            readModbusData(remoteEVCS, CHARGER_STATUS_REG, chargerStatus);
            xSemaphoreGive(modbusMutex);
        }

        if (cerboConnected) {
            Serial.println("Re-fetching CERBO GX data...");
            xSemaphoreTake(modbusMutex, portMAX_DELAY);
            readModbusData(remoteCERBO, PYLONTECH_SOC_REG, PylontechSOC, CERBO_UNIT_ID_TEMP);
            readModbusData(remoteCERBO, WATER_TEMP_REG, waterTemperature, CERBO_UNIT_ID_TEMP);
            readModbusData(remoteCERBO, DC_PV_POWER_REG, dcPvPower, CERBO_UNIT_ID_TEMP);
            for (int i = 0; i < 3; i++) {
                readModbusData(remoteCERBO, AC_PV_POWER_REGS[i], acPvPower[i], CERBO_UNIT_ID_TEMP);
            }
            xSemaphoreGive(modbusMutex);
        }

        // Aktualisiere das Display, um die neuen Werte anzuzeigen
        displayData();
    }
}


void setup() {
  Serial.begin(115200);
  connectToWiFi();

  drawWiFiIcon(WiFi.status() == WL_CONNECTED); 


    // Initialisiere NTP und Zeitzone
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println(&timeinfo, "Time initialized: %Y-%m-%d %H:%M:%S");
    } else {
        Serial.println("Failed to obtain time. Check WiFi and NTP settings.");
    }


  // Configure OTA updates
    ArduinoOTA.setHostname("WT32-OTA-Display");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("Start updating " + type);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("Update complete.");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });

    ArduinoOTA.begin();
    Serial.println("Ready for OTA updates");

  modbusMutex = xSemaphoreCreateMutex();
  modbusWriteQueue = xQueueCreate(10, sizeof(ModbusWriteRequest));
  
  lcd.init();

  if (lcd.width() < lcd.height()) lcd.setRotation(lcd.getRotation() ^ 1);

  lcd.fillScreen(TFT_WHITE);
  lcd.setTextColor(TFT_BLACK);
  lcd.setFont(&lgfx::v1::fonts::FreeSansBold12pt7b);
  
  
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  autoAdjustBrightness();
  fetchTibberPrices();  // Preise beim Start laden
  drawTibberPrice();    // Tibber-Preis direkt nach Abruf anzeigen
  switchTab(1); // Initialize with Tab 1

  mb.client();

  xTaskCreatePinnedToCore(modbusTask, "Modbus Task", TASK_STACK_SIZE, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(modbusWriteTask, "Modbus Write Task", TASK_STACK_SIZE, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(touchTask, "Touch Task", TASK_STACK_SIZE, NULL, 2, NULL, 1); 
}




void loop() {
    // Handle OTA updates
    ArduinoOTA.handle();

    // Timeout check for turning off the display
    if (displayOn && (millis() - lastInteractionTime > TFT_OFF_DELAY)) {
        Serial.printf("Inactivity detected. Turning off display. Last interaction time: %lu, Current time: %lu\n", lastInteractionTime, millis());
        turnOffDisplay();
    }

    // Überprüfe WLAN alle 5 Sekunden
    if (millis() - lastWiFiCheck > wifiCheckInterval) {
        lastWiFiCheck = millis();
        Serial.println("Checking WiFi connection...");
        checkWiFiConnection();
    }

 // Array verschieben, wenn Mitternacht erreicht wurde
    static int lastCheckedHour = -1; // Letzte geprüfte Stunde
    int currentHour = getCurrentHour(); // Nutze die Helper-Funktion

    if (currentHour == 0 && lastCheckedHour != 0) {
        Serial.println("Mitternacht erreicht, Preise verschieben...");
        shiftTibberPrices(); // Array um 12 Stunden verschieben
        lastCheckedHour = currentHour; // Aktualisiere die letzte geprüfte Stunde
    }

    // Stündliche Aktualisierung des aktuellen Strompreises
    static int lastUpdatedHour = -1;  // Letzte aktualisierte Stunde
    if (getCurrentHour() != lastUpdatedHour) {  // Nur wenn sich die Stunde ändert
        lastUpdatedHour = getCurrentHour();
        updateCurrentElectricityPrice();  // Aktualisiere den aktuellen Strompreis
        if (displayOn) {
            drawTibberPrice();  // Preis auf dem Display aktualisieren
        }
        Serial.printf("Current electricity price updated to: %.2f EUR\n", currentElectricityPrice);
    }

    // Aktualisiere Anzeige nur bei Preisänderung
    static float lastDisplayedPrice = -1;  // Letzter angezeigter Preis
    if (currentElectricityPrice != lastDisplayedPrice) {
        lastDisplayedPrice = currentElectricityPrice;
        if (displayOn) {
            drawTibberPrice();  // Preisanzeige aktualisieren
        }
        Serial.printf("Displayed Tibber price updated to: %.2f EUR\n", currentElectricityPrice);
    }

    // Tägliche Aktualisierung der Preise um 13:30 Uhr
    checkAndFetchTibberPrices();
}
