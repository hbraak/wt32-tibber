#include "vrm.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

float vrmSolarYield = 0.0;
float vrmConsumption = 0.0;
float vrmGridToConsumer = 0.0;
float vrmGridToGrid = 0.0;
float vrmSelfConsumption = 0.0;
float vrmNetGrid = 0.0;
bool  vrmDataLoaded = false;

static String vrmToken = "";
static unsigned long tokenFetchedAt = 0;

void fetchVrmToken() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin("https://vrmapi.victronenergy.com/v2/auth/login");
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String body = "{\"username\":\"" + String(VRM_USERNAME) + "\",\"password\":\"" + String(VRM_PASSWORD) + "\"}";
    int httpCode = http.POST(body);

    if (httpCode == 200) {
        String response = http.getString();
        DynamicJsonDocument* doc = new DynamicJsonDocument(2048);
        if (doc) {
            if (!deserializeJson(*doc, response)) {
                const char* t = (*doc)["token"];
                if (t) {
                    vrmToken = String(t);
                    tokenFetchedAt = millis();
                    Serial.println("VRM token obtained.");
                }
            }
            delete doc;
        }
    } else {
        Serial.printf("VRM login error: HTTP %d\n", httpCode);
    }
    http.end();
}

void fetchVrmDailyStats() {
    if (WiFi.status() != WL_CONNECTED) return;

    // Refresh token if older than 20 hours or empty
    if (vrmToken.length() == 0 || (millis() - tokenFetchedAt > 72000000UL)) {
        fetchVrmToken();
        if (vrmToken.length() == 0) return;
    }

    // Calculate start of today (UTC+1 for CET)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    
    // Start of today in local time → epoch
    struct tm startOfDay = timeinfo;
    startOfDay.tm_hour = 0;
    startOfDay.tm_min = 0;
    startOfDay.tm_sec = 0;
    time_t startEpoch = mktime(&startOfDay);
    time_t nowEpoch = mktime(&timeinfo);

    String url = "https://vrmapi.victronenergy.com/v2/installations/"
                 + String(VRM_SITE_ID)
                 + "/stats?type=custom&start=" + String((unsigned long)startEpoch)
                 + "&end=" + String((unsigned long)nowEpoch)
                 + "&attributeCodes[]=total_solar_yield"
                 + "&attributeCodes[]=total_consumption"
                 + "&attributeCodes[]=grid_history_to"
                 + "&attributeCodes[]=grid_history_from";

    HTTPClient http;
    http.begin(url);
    http.addHeader("X-Authorization", "Bearer " + vrmToken);
    http.setTimeout(10000);

    int httpCode = http.GET();
    if (httpCode == 200) {
        String response = http.getString();
        DynamicJsonDocument* doc = new DynamicJsonDocument(8192);
        if (doc) {
            if (!deserializeJson(*doc, response)) {
                // Sum up hourly values
                float solar = 0, consumption = 0, gridFrom = 0, gridTo = 0;

                JsonArray solarArr = (*doc)["records"]["total_solar_yield"];
                for (JsonArray entry : solarArr) solar += entry[1].as<float>();

                JsonArray consArr = (*doc)["records"]["total_consumption"];
                for (JsonArray entry : consArr) consumption += entry[1].as<float>();

                JsonArray fromArr = (*doc)["records"]["grid_history_from"];
                for (JsonArray entry : fromArr) gridFrom += entry[1].as<float>();

                JsonArray toArr = (*doc)["records"]["grid_history_to"];
                for (JsonArray entry : toArr) gridTo += entry[1].as<float>();

                vrmSolarYield = solar;
                vrmConsumption = consumption;
                vrmGridToConsumer = gridFrom;
                vrmGridToGrid = gridTo;
                vrmSelfConsumption = (solar > 0.1) ? ((solar - gridTo) / solar * 100.0) : 0.0;
                vrmNetGrid = gridFrom - gridTo;  // positive = net import, negative = net export
                vrmDataLoaded = true;

                Serial.printf("VRM: Solar=%.1f Cons=%.1f From=%.1f To=%.1f Self=%.0f%%\n",
                              solar, consumption, gridFrom, gridTo, vrmSelfConsumption);
            }
            delete doc;
        }
    } else if (httpCode == 401) {
        Serial.println("VRM token expired, refreshing...");
        vrmToken = "";
    } else {
        Serial.printf("VRM stats error: HTTP %d\n", httpCode);
    }
    http.end();
}
