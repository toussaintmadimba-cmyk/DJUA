#include "internet.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "../config.h"

// =====================================================
// IOT BOX - COMMUNICATION BACKEND
// =====================================================

static unsigned long lastWiFiRetry = 0;


// =====================================================
// CONNEXION WIFI
// =====================================================

void initInternet()
{
    Serial.println();
    Serial.print("[WiFi] Connexion a ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    unsigned long start = millis();

    // Timeout de 15 secondes
    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - start < 15000
    )
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("[WiFi] CONNECTE");

        Serial.print("[WiFi] IP : ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("[WiFi] CONNEXION IMPOSSIBLE");
    }
}


// =====================================================
// RECONNEXION
// =====================================================

void updateInternet()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    unsigned long now = millis();

    if (now - lastWiFiRetry >= WIFI_RETRY_INTERVAL_MS)
    {
        lastWiFiRetry = now;

        Serial.println("[WiFi] Tentative de reconnexion...");

        WiFi.disconnect();
        WiFi.begin(
            WIFI_SSID,
            WIFI_PASSWORD
        );
    }
}


// =====================================================
// ETAT CONNEXION
// =====================================================

bool isInternetConnected()
{
    return WiFi.status() == WL_CONNECTED;
}


// =====================================================
// ENVOI AU BACKEND
// =====================================================

bool sendTelemetryToBackend(const TelemetryData& data)
{
    if (!isInternetConnected())
    {
        Serial.println(
            "[HTTP] Impossible : WiFi non connecte."
        );

        return false;
    }

    // =================================================
    // CONSTRUCTION DU JSON
    // =================================================

    StaticJsonDocument<1024> doc;

    // Identifiant attendu par le backend
    doc["kit_id"] = DEVICE_ID;

    // GPS
    doc["latitude"] = data.latitude;
    doc["longitude"] = data.longitude;

    // =================================================
    // TIMESTAMP
    //
    // On conserve pour l'instant le meme format
    // utilise par le firmware deja valide.
    // =================================================

    char timeBuffer[32];

    unsigned long seconds = millis() / 1000;

    snprintf(
        timeBuffer,
        sizeof(timeBuffer),
        "T%02lu:%02lu:%02luZ",
        (seconds / 3600) % 24,
        (seconds / 60) % 60,
        seconds % 60
    );

    doc["timestamp"] = timeBuffer;

    doc["interval_seconds"] =
        data.intervalSeconds;

    // =================================================
    // SOLAR
    // =================================================

    JsonObject solar =
        doc.createNestedObject("solar");

    solar["voltage_v"] =
        data.solarVoltage;

    solar["current_a"] =
        data.solarCurrent;

    solar["power_w"] =
        data.solarPower;

    solar["energy_interval_wh"] =
        data.solarEnergyIntervalWh;

    // =================================================
    // BATTERY - VALEURS REELLES INA219
    // =================================================

    JsonObject battery =
        doc.createNestedObject("battery");

    battery["voltage_v"] =
        data.batteryVoltage;

    battery["current_a"] =
        data.batteryCurrent;

    battery["power_w"] =
        data.batteryPower;

    // =================================================
    // DC LOAD
    // =================================================

    JsonObject dcLoad =
        doc.createNestedObject("dc_load");

    dcLoad["voltage_v"] =
        data.dcLoadVoltage;

    dcLoad["current_a"] =
        data.dcLoadCurrent;

    dcLoad["power_w"] =
        data.dcLoadPower;

    dcLoad["energy_interval_wh"] =
        data.dcLoadEnergyIntervalWh;

    // =================================================
    // AC LOAD
    // =================================================

    JsonObject acLoad =
        doc.createNestedObject("ac_load");

    acLoad["voltage_v"] =
        data.acLoadVoltage;

    acLoad["current_a"] =
        data.acLoadCurrent;

    acLoad["apparent_power_va"] =
        data.acApparentPower;

    acLoad["energy_interval_vah"] =
        data.acEnergyIntervalVAh;

    // =================================================
    // CONVERSION JSON -> STRING
    // =================================================

    String payload;

    serializeJson(
        doc,
        payload
    );

    Serial.println();
    Serial.println("========== TELEMETRIE ==========");
    Serial.println(payload);
    Serial.println("================================");

    // =================================================
    // HTTP POST
    // =================================================

    WiFiClient client;
    HTTPClient http;

    if (!http.begin(client, BACKEND_URL))
    {
        Serial.println(
            "[HTTP] Impossible d'ouvrir le endpoint."
        );

        return false;
    }

    http.setTimeout(10000);

    http.addHeader(
        "Content-Type",
        "application/json"
    );

    http.addHeader(
        "x-device-token",
        IOT_API_KEY
    );

    Serial.println("[HTTP] POST vers backend...");

    int httpCode =
        http.POST(payload);

    if (httpCode <= 0)
    {
        Serial.print("[HTTP] Erreur : ");
        Serial.println(
            http.errorToString(httpCode)
        );

        http.end();

        return false;
    }

    Serial.print("[HTTP] Code : ");
    Serial.println(httpCode);

    String response =
        http.getString();

    Serial.print("[HTTP] Reponse : ");
    Serial.println(response);

    bool success =
        httpCode >= 200 &&
        httpCode < 300;

    http.end();

    return success;
}