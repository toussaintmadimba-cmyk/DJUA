#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

#include "../geofencing/geofence.h"
#include "../telemetry/telemetry.h"

// Configures the MQTT client. Call after Wi-Fi initialisation.
void initMQTT();

// Maintains the MQTT connection. Call continuously from loop().
void updateMQTT();

// Publishes a telemetry record on the device-specific telemetry topic.
bool sendTelemetryToMQTT(const TelemetryData& data);

// Publie le dernier controle geofence et, lors d'une transition, un evenement.
bool sendGeofenceToMQTT(
    const GeofenceResult& result,
    const char* timestamp,
    bool timestampValid
);

bool isMQTTConnected();

#endif
