#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

#include "../geofencing/geofence.h"
#include "../telemetry/telemetry.h"

// Configures the MQTT QoS 1 client and its persistent outgoing queue.
// Call after Wi-Fi initialisation.
void initMQTT();

// Sends queued messages once the broker is connected. Call continuously from loop().
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

// Number of sensor/geofence records waiting for a broker acknowledgement.
uint16_t pendingMQTTMessageCount();

#endif
