#ifndef GEOFENCE_H
#define GEOFENCE_H

#include <Arduino.h>

#include "../sensors/gps.h"

enum GeofenceState : uint8_t
{
    GEOFENCE_UNKNOWN,
    GEOFENCE_INSIDE,
    GEOFENCE_OUTSIDE
};

enum GeofenceEventType : uint8_t
{
    GEOFENCE_EVENT_NONE,
    GEOFENCE_EVENT_EXIT,
    GEOFENCE_EVENT_ENTER
};

struct GeofenceResult
{
    GeofenceState state;
    float distanceMeters;
    bool exitEvent;
    bool enterEvent;

    GeofenceEventType eventType;
    bool positionUsable;
    double latitude;
    double longitude;
    const char* deviceId;

    GeofenceState candidateState;
    uint16_t confirmationCount;
};

// Reinitialise la machine d'etat a UNKNOWN.
void initGeofence();

// Evalue une position recente fournie par le module GPS.
GeofenceResult updateGeofence(const GPSData& position);

// Expose le calcul pour les tests avec des coordonnees simulees.
float calculateGeofenceDistanceMeters(
    double latitude1,
    double longitude1,
    double latitude2,
    double longitude2
);

const char* geofenceStateToString(GeofenceState state);
const char* geofenceEventToString(GeofenceEventType eventType);

#endif
