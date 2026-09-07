#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

// =====================================================
// IOT BOX - DONNEES GPS
// =====================================================

struct GPSData
{
    double latitude;
    double longitude;
    bool valid;
};

void initGPS();

// A appeler continuellement dans loop()
void updateGPS();

// Retourne la derniere position seulement si elle est valide et recente
GPSData readGPS();

// Indique si le GPS dispose actuellement d'une position valide et recente
bool hasGPSFix();

#endif
