#include "gps.h"

#include <TinyGPSPlus.h>

#include "../pins.h"
#include "../config.h"

// =====================================================
// IOT BOX - NEO-6M
// =====================================================

static TinyGPSPlus gps;

// UART matériel numéro 2 de l'ESP32
static HardwareSerial gpsSerial(2);

// INITIALISATION

void initGPS()
{
    gpsSerial.begin(
        GPS_BAUD_RATE,
        SERIAL_8N1,
        GPS_RX_PIN,
        GPS_TX_PIN
    );

    Serial.println("[GPS] NEO-6M initialise.");
    Serial.print("[GPS] RX ESP32 : GPIO ");
    Serial.println(GPS_RX_PIN);

    Serial.print("[GPS] TX ESP32 : GPIO ");
    Serial.println(GPS_TX_PIN);
}

// MISE A JOUR

void updateGPS()
{
    /*
     * Important :
     * TinyGPSPlus doit recevoir continuellement les
     * caracteres provenant du NEO-6M.
     */
    while (gpsSerial.available() > 0)
    {
        gps.encode(gpsSerial.read());
    }
}

// LECTURE

GPSData readGPS()
{
    GPSData data;

    data.latitude = 0.0;
    data.longitude = 0.0;
    data.valid = false;

    if (!gps.location.isValid())
    {
        Serial.println("[GPS] Sans FIX - aucune position valide.");
        return data;
    }

    const unsigned long locationAge = gps.location.age();

    if (locationAge > GPS_MAX_AGE_MS)
    {
        Serial.print("[GPS] FIX ancien/perime (age ");
        Serial.print(locationAge);
        Serial.print(" ms, limite ");
        Serial.print(GPS_MAX_AGE_MS);
        Serial.println(" ms).");
        return data;
    }

    data.latitude = gps.location.lat();
    data.longitude = gps.location.lng();
    data.valid = true;

    Serial.print("[GPS] FIX valide et recent (age ");
    Serial.print(locationAge);
    Serial.println(" ms).");

    return data;
}

// ETAT DU GPS

bool hasGPSFix()
{
    return
        gps.location.isValid() &&
        gps.location.age() <= GPS_MAX_AGE_MS;
}
