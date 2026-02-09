#include "tibber.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// Definitionen der Variablen
float currentElectricityPrice = 0.0;
float tibberPrices[48] = {0.0};  // Speicher für 48 Preise (24 Stunden für 2 Tage)
bool pricesLoaded = false;
const char* tibberAPI = "https://api.tibber.com/v1-beta/gql";
const char* tibberToken = "ha0Aetr1iaF9oY1ZsxF3eEBZYPYWpq7rjv0NLA0mmu8";

// Helper function: Get current time in HHMM format
int getCurrentTimeInHHMM() {
    return getCurrentHour() * 100 + minute();
}

void fetchTibberPrices() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(tibberAPI);
        http.addHeader("Authorization", String("Bearer ") + tibberToken);
        http.addHeader("Content-Type", "application/json");

        // GraphQL-Query mit erweitertem Payload für Verbrauchsdaten
        String payload = R"(
        {
            "query": "{ 
                viewer { 
                    homes { 
                        currentSubscription { 
                            priceInfo { 
                                today { total } 
                                tomorrow { total } 
                            } 
                            consumption { 
                                nodes { 
                                    consumption 
                                    cost 
                                } 
                            } 
                        } 
                    } 
                } 
            }"
        })";

        int httpResponseCode = http.POST(payload);

        Serial.println("Payload:");
        Serial.println(payload);

        if (httpResponseCode == 200) {
            String response = http.getString();
            Serial.println("Tibber API response:");
            Serial.println(response);

            // JSON-Daten parsen
            StaticJsonDocument<8192> doc; // Erhöhter Speicherplatz für zusätzliche Daten
            DeserializationError error = deserializeJson(doc, response);
            if (error) {
                Serial.print("JSON deserialization failed: ");
                Serial.println(error.c_str());
                return;
            }

            // Stundenpreise für heute und morgen extrahieren
            JsonArray todayPrices = doc["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["today"].as<JsonArray>();
            JsonArray tomorrowPrices = doc["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["tomorrow"].as<JsonArray>();

            int index = 0;
            for (JsonObject priceObj : todayPrices) {
                tibberPrices[index++] = priceObj["total"].as<float>();
            }
            for (JsonObject priceObj : tomorrowPrices) {
                tibberPrices[index++] = priceObj["total"].as<float>();
            }

            // Fehlende Werte auffüllen
            while (index < 48) {
                tibberPrices[index++] = 0.0;
            }

            // Den aktuellen Strompreis setzen
            currentElectricityPrice = tibberPrices[getCurrentHour()];

            // Verbrauchsdaten extrahieren
            float totalConsumption = 0.0;
            float totalCost = 0.0;

            // Verfügbarkeit der Verbrauchsdaten prüfen
            if (doc["data"]["viewer"]["homes"][0]["currentSubscription"]["consumption"]["nodes"].is<JsonArray>()) {
                JsonArray consumptionNodes = doc["data"]["viewer"]["homes"][0]["currentSubscription"]["consumption"]["nodes"].as<JsonArray>();

                for (JsonObject node : consumptionNodes) {
                    totalConsumption += node["consumption"].as<float>();
                    totalCost += node["cost"].as<float>();
                }
            }

            // Debug-Ausgaben
            Serial.println("Tibber prices updated:");
            for (int i = 0; i < 48; i++) {
                Serial.printf("Hour %d: %.2f EUR\n", i, tibberPrices[i]);
            }
            Serial.printf("Tagesverbrauch: %.2f kWh\n", totalConsumption);
            Serial.printf("Tageskosten: %.2f EUR\n", totalCost);

            pricesLoaded = true;

        } else {
            Serial.printf("Error fetching Tibber prices, HTTP code: %d\n", httpResponseCode);
            Serial.println(http.getString()); // Ausgabe der Serverantwort für Debugging
        }

        http.end();
    } else {
        Serial.println("Error: Not connected to WiFi.");
    }
}


void updateCurrentElectricityPrice() {
    if (pricesLoaded) {
        currentElectricityPrice = tibberPrices[getCurrentHour()];
        Serial.printf("Updated current electricity price to %.2f EUR\n", currentElectricityPrice);
    } else {
        Serial.println("Prices not loaded yet. Cannot update current price.");
    }
}

void shiftTibberPrices() {
    for (int i = 24; i < 48; i++) {
        tibberPrices[i - 24] = tibberPrices[i];
    }
    for (int i = 24; i < 48; i++) {
        tibberPrices[i] = 0.0;
    }
    Serial.println("Tibber prices shifted:");
    for (int i = 0; i < 48; i++) {
        Serial.printf("Hour %d: %.2f EUR\n", i, tibberPrices[i]);
    }
}

int getCurrentHour() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        return timeinfo.tm_hour;
    } else {
        Serial.println("Failed to get local time.");
        return -1;
    }
}

void checkAndFetchTibberPrices() {
    static bool fetchedToday = false;
    int currentTime = getCurrentTimeInHHMM();

    if (currentTime >= 1330 && !fetchedToday) {
        Serial.println("Fetching new Tibber prices for the next cycle...");
        fetchTibberPrices();
        fetchedToday = true;
    } else if (currentTime < 1330) {
        fetchedToday = false;  // Flag für den nächsten Tag zurücksetzen
    }
}