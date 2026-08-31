#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

#include "../telemetry/telemetry.h"

// Configures the MQTT client. Call after Wi-Fi initialisation.
void initMQTT();

// Maintains the MQTT connection. Call continuously from loop().
void updateMQTT();

// Publishes a telemetry record on the device-specific telemetry topic.
bool sendTelemetryToMQTT(const TelemetryData& data);

bool isMQTTConnected();

#endif
