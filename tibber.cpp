#include "tibber.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

float currentElectricityPrice = 0.0;
float tibberPrices[48] = {0.0};
static bool pricesLoaded = false;

int getCurrentHour() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        return timeinfo.tm_hour;
    }
    Serial.println("Failed to get local time.");
    return -1;
}

static int getCurrentTimeInHHMM() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        return timeinfo.tm_hour * 100 + timeinfo.tm_min;
    }
    return -1;
}

void fetchTibberPrices() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("fetchTibberPrices: No WiFi");
        return;
    }

    HTTPClient http;
    http.begin(TIBBER_API_URL);
    http.addHeader("Authorization", String("Bearer ") + TIBBER_API_TOKEN);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String payload = R"({"query":"{ viewer { homes { currentSubscription { priceInfo { today { total } tomorrow { total } } } } } }"})";

    int httpCode = http.POST(payload);

    if (httpCode == 200) {
        String response = http.getString();
        
        // Use DynamicJsonDocument to avoid stack overflow
        DynamicJsonDocument* doc = new DynamicJsonDocument(4096);
        if (!doc) {
            Serial.println("Failed to allocate JSON document");
            http.end();
            return;
        }
        
        DeserializationError error = deserializeJson(*doc, response);
        if (error) {
            Serial.printf("JSON parse error: %s\n", error.c_str());
            delete doc;
            http.end();
            return;
        }

        JsonArray todayPrices = (*doc)["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["today"].as<JsonArray>();
        JsonArray tomorrowPrices = (*doc)["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["tomorrow"].as<JsonArray>();

        int index = 0;
        for (JsonObject p : todayPrices) {
            if (index < 48) tibberPrices[index++] = p["total"].as<float>();
        }
        for (JsonObject p : tomorrowPrices) {
            if (index < 48) tibberPrices[index++] = p["total"].as<float>();
        }
        while (index < 48) tibberPrices[index++] = 0.0;

        int hour = getCurrentHour();
        if (hour >= 0 && hour < 48) {
            currentElectricityPrice = tibberPrices[hour];
        }

        pricesLoaded = true;
        Serial.println("Tibber prices updated successfully.");
        
        delete doc;
    } else {
        Serial.printf("Tibber API error: HTTP %d\n", httpCode);
    }

    http.end();
}

void updateCurrentElectricityPrice() {
    if (!pricesLoaded) return;
    int hour = getCurrentHour();
    if (hour >= 0 && hour < 48) {
        currentElectricityPrice = tibberPrices[hour];
    }
}

void shiftTibberPrices() {
    for (int i = 0; i < 24; i++) {
        tibberPrices[i] = tibberPrices[i + 24];
    }
    for (int i = 24; i < 48; i++) {
        tibberPrices[i] = 0.0;
    }
    Serial.println("Tibber prices shifted (midnight).");
}

void checkAndFetchTibberPrices() {
    static bool fetchedToday = false;
    int currentTime = getCurrentTimeInHHMM();
    if (currentTime < 0) return;

    if (currentTime >= 1330 && !fetchedToday) {
        Serial.println("Fetching new Tibber prices...");
        fetchTibberPrices();
        fetchedToday = true;
    } else if (currentTime < 1330) {
        fetchedToday = false;
    }
}
