#include "geofence.h"

#include <math.h>

#include "../config.h"

static_assert(GEOFENCE_ENTER_RADIUS_M < GEOFENCE_EXIT_RADIUS_M,
              "Le rayon d'entree doit etre inferieur au rayon de sortie.");
static_assert(GEOFENCE_CONFIRM_COUNT > 0,
              "Le nombre de confirmations doit etre superieur a zero.");

namespace
{
constexpr double EARTH_RADIUS_METERS = 6371000.0;
constexpr double DEGREES_TO_RADIANS =
    3.14159265358979323846 / 180.0;

GeofenceState currentState = GEOFENCE_UNKNOWN;
GeofenceState candidateState = GEOFENCE_UNKNOWN;
uint16_t candidateCount = 0;

void resetCandidate()
{
    candidateState = GEOFENCE_UNKNOWN;
    candidateCount = 0;
}

uint16_t confirmCandidate(GeofenceState requestedState)
{
    if (candidateState != requestedState)
    {
        candidateState = requestedState;
        candidateCount = 1;
    }
    else if (candidateCount < GEOFENCE_CONFIRM_COUNT)
    {
        candidateCount++;
    }

    return candidateCount;
}

bool isPositionUsable(const GPSData& position)
{
    const bool coordinatesInRange =
        position.latitude >= -90.0 &&
        position.latitude <= 90.0 &&
        position.longitude >= -180.0 &&
        position.longitude <= 180.0;

    // Dans le firmware DJUA, 0.0/0.0 represente une position indisponible.
    const bool isZeroPosition =
        position.latitude == 0.0 &&
        position.longitude == 0.0;

    return position.valid && coordinatesInRange && !isZeroPosition;
}

GeofenceResult makeBaseResult(const GPSData& position)
{
    GeofenceResult result = {};
    result.state = currentState;
    result.distanceMeters = -1.0F;
    result.exitEvent = false;
    result.enterEvent = false;
    result.eventType = GEOFENCE_EVENT_NONE;
    result.positionUsable = isPositionUsable(position);
    result.latitude = position.latitude;
    result.longitude = position.longitude;
    result.deviceId = DEVICE_ID;
    result.candidateState = GEOFENCE_UNKNOWN;
    result.confirmationCount = 0;
    return result;
}
} // namespace

float calculateGeofenceDistanceMeters(
    double latitude1,
    double longitude1,
    double latitude2,
    double longitude2
)
{
    const double latitude1Radians = latitude1 * DEGREES_TO_RADIANS;
    const double latitude2Radians = latitude2 * DEGREES_TO_RADIANS;
    const double latitudeDelta =
        (latitude2 - latitude1) * DEGREES_TO_RADIANS;
    const double longitudeDelta =
        (longitude2 - longitude1) * DEGREES_TO_RADIANS;

    const double sinHalfLatitude = sin(latitudeDelta / 2.0);
    const double sinHalfLongitude = sin(longitudeDelta / 2.0);

    double haversine =
        sinHalfLatitude * sinHalfLatitude +
        cos(latitude1Radians) * cos(latitude2Radians) *
            sinHalfLongitude * sinHalfLongitude;

    // Protege sqrt() des petites erreurs d'arrondi flottant.
    if (haversine < 0.0)
    {
        haversine = 0.0;
    }
    else if (haversine > 1.0)
    {
        haversine = 1.0;
    }

    const double angularDistance =
        2.0 * atan2(sqrt(haversine), sqrt(1.0 - haversine));

    return static_cast<float>(EARTH_RADIUS_METERS * angularDistance);
}

void initGeofence()
{
    currentState = GEOFENCE_UNKNOWN;
    resetCandidate();
}

GeofenceResult updateGeofence(const GPSData& position)
{
    GeofenceResult result = makeBaseResult(position);

    if (!result.positionUsable)
    {
        // Une interruption GPS casse une serie de confirmations, mais ne
        // modifie jamais le dernier etat geofence confirme.
        resetCandidate();
        return result;
    }

    result.distanceMeters = calculateGeofenceDistanceMeters(
        GEOFENCE_CENTER_LAT,
        GEOFENCE_CENTER_LON,
        position.latitude,
        position.longitude
    );

    if (currentState == GEOFENCE_UNKNOWN)
    {
        if (result.distanceMeters <= GEOFENCE_ENTER_RADIUS_M)
        {
            currentState = GEOFENCE_INSIDE;
        }
        else if (result.distanceMeters >= GEOFENCE_EXIT_RADIUS_M)
        {
            currentState = GEOFENCE_OUTSIDE;
        }

        resetCandidate();
        result.state = currentState;
        return result;
    }

    if (currentState == GEOFENCE_INSIDE)
    {
        if (result.distanceMeters >= GEOFENCE_EXIT_RADIUS_M)
        {
            const uint16_t confirmations =
                confirmCandidate(GEOFENCE_OUTSIDE);

            result.candidateState = GEOFENCE_OUTSIDE;
            result.confirmationCount = confirmations;

            if (confirmations >= GEOFENCE_CONFIRM_COUNT)
            {
                currentState = GEOFENCE_OUTSIDE;
                result.eventType = GEOFENCE_EVENT_EXIT;
                result.exitEvent = true;
                resetCandidate();
            }
        }
        else
        {
            // Position INSIDE ou dans la zone tampon : rester INSIDE.
            resetCandidate();
        }
    }
    else if (currentState == GEOFENCE_OUTSIDE)
    {
        if (result.distanceMeters <= GEOFENCE_ENTER_RADIUS_M)
        {
            const uint16_t confirmations =
                confirmCandidate(GEOFENCE_INSIDE);

            result.candidateState = GEOFENCE_INSIDE;
            result.confirmationCount = confirmations;

            if (confirmations >= GEOFENCE_CONFIRM_COUNT)
            {
                currentState = GEOFENCE_INSIDE;
                result.eventType = GEOFENCE_EVENT_ENTER;
                result.enterEvent = true;
                resetCandidate();
            }
        }
        else
        {
            // Position OUTSIDE ou dans la zone tampon : rester OUTSIDE.
            resetCandidate();
        }
    }

    result.state = currentState;
    return result;
}

const char* geofenceStateToString(GeofenceState state)
{
    switch (state)
    {
        case GEOFENCE_INSIDE:
            return "INSIDE";

        case GEOFENCE_OUTSIDE:
            return "OUTSIDE";

        case GEOFENCE_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

const char* geofenceEventToString(GeofenceEventType eventType)
{
    switch (eventType)
    {
        case GEOFENCE_EVENT_EXIT:
            return "GEOFENCE_EXIT";

        case GEOFENCE_EVENT_ENTER:
            return "GEOFENCE_ENTER";

        case GEOFENCE_EVENT_NONE:
        default:
            return "NONE";
    }
}
