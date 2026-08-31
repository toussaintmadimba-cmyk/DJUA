#include <Arduino.h>
#include "src/config.h"
#include "src/sensors/power_sensor.h"
#include "src/sensors/gps.h"
#include "src/telemetry/telemetry.h"
#include "src/communication/internet.h"
#include "src/communication/mqtt.h"
static unsigned long lastTelemetryTime = 0;
// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("          IOT BOX V1");
    Serial.println("=================================");

    Serial.print("KIT ID : ");
    Serial.println(DEVICE_ID);

    // =================================================
    // INA219
    // =================================================

    Serial.println();
    Serial.println("[INA219] Initialisation...");

    if (initPowerSensor())
    {
        Serial.println("[INA219] Batterie OK");
    }
    else
    {
        Serial.println("[INA219] ERREUR");
    }

    // =================================================
    // GPS
    // =================================================

    Serial.println();
    initGPS();

    // =================================================
    // INTERNET
    // =================================================

    initInternet();
    initMQTT();

    lastTelemetryTime = millis();

    Serial.println();
    Serial.println("[SYSTEM] Initialisation terminee.");
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    
    updateGPS();

    
    updateInternet();
    updateMQTT();

    unsigned long now = millis();

    if (
        now - lastTelemetryTime <
        TELEMETRY_INTERVAL_MS
    )
    {
        return;
    }

    unsigned long elapsed =
        now - lastTelemetryTime;

    lastTelemetryTime = now;

    unsigned long intervalSeconds =
        elapsed / 1000;

    if (intervalSeconds == 0)
    {
        intervalSeconds =
            TELEMETRY_INTERVAL_MS / 1000;
    }

    Serial.println();
    Serial.println("=================================");
    Serial.println("        NOUVELLE MESURE");
    Serial.println("=================================");

    // =================================================
    // BATTERIE REELLE
    // =================================================

    PowerData battery =
        readPowerSensor();

    if (!battery.valid)
    {
        Serial.println(
            "[INA219] Mesure invalide."
        );

        
        return;
    }

    Serial.print("[BAT] Tension : ");
    Serial.print(battery.voltage, 2);
    Serial.println(" V");

    Serial.print("[BAT] Courant : ");
    Serial.print(battery.current, 3);
    Serial.println(" A");

    Serial.print("[BAT] Puissance : ");
    Serial.print(battery.power, 2);
    Serial.println(" W");

    // =================================================
    // GPS REEL
    // =================================================

    GPSData location =
        readGPS();

    if (location.valid)
    {
        Serial.print("[GPS] Latitude : ");
        Serial.println(
            location.latitude,
            6
        );

        Serial.print("[GPS] Longitude : ");
        Serial.println(
            location.longitude,
            6
        );
    }
    else
    {
        Serial.println(
            "[GPS] Pas de FIX - envoi 0.0 / 0.0"
        );
    }

    // =================================================
    // CONSTRUCTION TELEMETRIE
    // =================================================

    TelemetryData telemetry =
        buildTelemetry(
            battery,
            location,
            intervalSeconds
        );

    // =================================================
    // ENVOI MQTT
    // =================================================

    sendTelemetryToMQTT(telemetry);

#if ENABLE_HTTP_BACKEND
    // =================================================
    // ENVOI BACKEND HTTP (optionnel)
    // =================================================

    if (sendTelemetryToBackend(telemetry))
    {
        Serial.println("[BACKEND] TELEMETRIE ACCEPTEE");
    }
    else
    {
        Serial.println("[BACKEND] ECHEC ENVOI");
    }
#endif
}
