#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>

#include "../sensors/power_sensor.h"
#include "../sensors/gps.h"
#include "../sensors/rtc_ds1302.h"

// =====================================================
// IOT BOX - STRUCTURE DE TELEMETRIE
// =====================================================

struct TelemetryData
{
    // HORODATAGE REEL DS1302 AU FORMAT ISO 8601
    char timestamp[32];
    bool timestampValid;

    // GPS REEL
    double latitude;
    double longitude;

    // BATTERIE REELLE - INA219
    float batteryVoltage;
    float batteryCurrent;
    float batteryPower;

    // PANNEAU SOLAIRE - PAS ENCORE MESURE
    float solarVoltage;
    float solarCurrent;
    float solarPower;
    float solarEnergyIntervalWh;

    // CHARGE AC - PAS ENCORE MESURE
    float acLoadVoltage;
    float acLoadCurrent;
    float acApparentPower;
    float acEnergyIntervalVAh;

    // Intervalle entre deux transmissions
    unsigned long intervalSeconds;
};


// Construit une telemetrie complete
TelemetryData buildTelemetry(
    const PowerData& battery,
    const GPSData& gps,
    const RTCData& rtc,
    unsigned long intervalSeconds
);

#endif
