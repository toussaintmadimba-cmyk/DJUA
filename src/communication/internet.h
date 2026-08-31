#ifndef INTERNET_H
#define INTERNET_H

#include <Arduino.h>

#include "../telemetry/telemetry.h"

// Initialise la connexion Wi-Fi
void initInternet();

// Gere la reconnexion Wi-Fi
void updateInternet();

// Indique si Internet/reseau Wi-Fi est disponible
bool isInternetConnected();

// Envoie une telemetrie au backend
bool sendTelemetryToBackend(const TelemetryData& data);

#endif