#include <Arduino.h>
#include "src/config.h"
#include "src/sensors/power_sensor.h"
#include "src/sensors/gps.h"
#include "src/sensors/rtc_ds1302.h"
#include "src/geofencing/geofence.h"
#include "src/telemetry/telemetry.h"
#include "src/communication/internet.h"
#include "src/communication/mqtt.h"
static unsigned long lastTelemetryTime = 0;
static unsigned long lastGeofenceCheckTime = 0;

static void reportGeofenceEvent(
    const GeofenceResult& result,
    const char* timestamp,
    bool timestampValid
)
{
    Serial.println(
        result.exitEvent
            ? "[ALERTE] GEOFENCE_EXIT"
            : "[INFO] GEOFENCE_ENTER"
    );

    Serial.print("[GEOFENCE_EVENT] device=");
    Serial.print(result.deviceId);
    Serial.print(" | type=");
    Serial.print(geofenceEventToString(result.eventType));
    Serial.print(" | timestamp=");
    Serial.print(timestampValid ? timestamp : "indisponible");
    Serial.print(" | latitude=");
    Serial.print(result.latitude, 6);
    Serial.print(" | longitude=");
    Serial.print(result.longitude, 6);
    Serial.print(" | distance_m=");
    Serial.println(result.distanceMeters, 1);

    // TODO: transmettre GEOFENCE_EXIT/GEOFENCE_ENTER via le module SMS.
}

static void updateGeofenceMonitoring()
{
    if (!GEOFENCE_ENABLED)
    {
        return;
    }

    const unsigned long now = millis();

    if (
        now - lastGeofenceCheckTime <
        GEOFENCE_CHECK_INTERVAL_MS
    )
    {
        return;
    }

    // Ne pas rejouer plusieurs controles sur la meme position apres un retard.
    lastGeofenceCheckTime = now;

    const GPSData position = readGPS();
    const GeofenceResult result = updateGeofence(position);

    const RTCData clock = readRTC();
    char timestamp[32] = {};
    const bool timestampValid = formatRTCTimestamp(
        clock,
        timestamp,
        sizeof(timestamp)
    );

    // Chaque controle est publie pour permettre le suivi d'un test reel.
    sendGeofenceToMQTT(result, timestamp, timestampValid);

    if (!result.positionUsable)
    {
        Serial.println(
            "[GEOFENCE] Verification ignoree : GPS invalide ou trop ancien"
        );
        return;
    }

    Serial.print("[GEOFENCE] GPS valide | Distance : ");
    Serial.print(result.distanceMeters, 1);
    Serial.print(" m | Etat : ");
    Serial.println(geofenceStateToString(result.state));

    if (result.confirmationCount > 0)
    {
        Serial.print("[GEOFENCE] Candidat ");
        Serial.print(geofenceStateToString(result.candidateState));
        Serial.print(' ');
        Serial.print(result.confirmationCount);
        Serial.print('/');
        Serial.println(GEOFENCE_CONFIRM_COUNT);
    }

    if (result.exitEvent || result.enterEvent)
    {
        reportGeofenceEvent(result, timestamp, timestampValid);
    }
}
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
    initGeofence();

    // =================================================
    // HORLOGE RTC DS1302
    // =================================================

    Serial.println();
    Serial.println("[DS1302] Initialisation...");

    if (initRTC())
    {
        Serial.println("[DS1302] HORLOGE OK");
    }
    else
    {
        Serial.println("[DS1302] ERREUR - MODE DE SECOURS ACTIF");
    }

    // =================================================
    // INTERNET
    // =================================================

    initInternet();
    initMQTT();

    lastTelemetryTime = millis();
    lastGeofenceCheckTime = millis();

    Serial.println();
    Serial.println("[SYSTEM] Initialisation terminee.");
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    
    updateGPS();

    // Priorite a la surveillance locale avant les transports reseau.
    updateGeofenceMonitoring();

    
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
            "[GPS] Position non exploitable - envoi 0.0 / 0.0"
        );
    }

    // =================================================
    // HORLOGE RTC DS1302
    // =================================================

    RTCData clock = readRTC();

    // =================================================
    // CONSTRUCTION TELEMETRIE
    // =================================================

    TelemetryData telemetry =
        buildTelemetry(
            battery,
            location,
            clock,
            intervalSeconds
        );

    if (telemetry.timestampValid)
    {
        Serial.print("[DS1302] Timestamp : ");
        Serial.println(telemetry.timestamp);
    }
    else
    {
        Serial.println(
            "[DS1302] Timestamp invalide - utilisation du secours."
        );
    }

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
