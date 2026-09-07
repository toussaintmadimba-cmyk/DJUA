#include "telemetry.h"

// =====================================================
// CONSTRUCTION DE LA TELEMETRIE
// =====================================================

TelemetryData buildTelemetry(
    const PowerData& battery,
    const GPSData& gps,
    const RTCData& rtc,
    unsigned long intervalSeconds
)
{
    TelemetryData data = {};

    // =================================================
    // HORODATAGE REEL DS1302
    // =================================================

    data.timestampValid = formatRTCTimestamp(
        rtc,
        data.timestamp,
        sizeof(data.timestamp)
    );

    if (!data.timestampValid)
    {
        data.timestamp[0] = '\0';
    }

    // =================================================
    // GPS REEL
    // =================================================

    if (gps.valid)
    {
        data.latitude = gps.latitude;
        data.longitude = gps.longitude;
    }
    else
    {
        /*
         * Pas de faux emplacement Kinshasa.
         *
         * Le backend actuel attend latitude/longitude,
         * donc 0.0 indique simplement qu'aucune
         * position GPS n'est disponible.
         */
        data.latitude = 0.0;
        data.longitude = 0.0;
    }

    // =================================================
    // BATTERIE REELLE - INA219
    // =================================================

    data.batteryVoltage = battery.voltage;
    data.batteryCurrent = battery.current;
    data.batteryPower = battery.power;

    // SOLAIRE

    data.solarVoltage = 0.0;
    data.solarCurrent = 0.0;
    data.solarPower = 0.0;
    data.solarEnergyIntervalWh = 0.0;

    // AC LOAD
   
    data.acLoadVoltage = 0.0;
    data.acLoadCurrent = 0.0;
    data.acApparentPower = 0.0;
    data.acEnergyIntervalVAh = 0.0;

    // INTERVALLE

    data.intervalSeconds = intervalSeconds;

    return data;
}
