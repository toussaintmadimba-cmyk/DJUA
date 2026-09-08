#include <Arduino.h>
#include <math.h>

#include "../../../src/config.h"
#include "../../../src/geofencing/geofence.h"

// Le sketch Arduino de test est autonome : il compile directement
// les implementations de production GPS et geofencing.
#include "../../../src/sensors/gps.cpp"
#include "../../../src/geofencing/geofence.cpp"

namespace
{
constexpr unsigned long TEST_SERIAL_BAUD = 115200UL;
constexpr double TEST_EARTH_RADIUS_METERS = 6371000.0;
constexpr double TEST_RADIANS_TO_DEGREES =
    180.0 / 3.14159265358979323846;

struct TestCounters
{
    uint16_t passed;
    uint16_t failed;
};

TestCounters counters = {0, 0};

void check(const char* name, bool condition)
{
    if (condition)
    {
        counters.passed++;
        Serial.print("[PASS] ");
    }
    else
    {
        counters.failed++;
        Serial.print("[FAIL] ");
    }

    Serial.println(name);
}

GPSData positionAtDistance(float distanceMeters)
{
    GPSData position = {};
    const double latitudeOffset =
        (distanceMeters / TEST_EARTH_RADIUS_METERS) *
        TEST_RADIANS_TO_DEGREES;

    position.latitude = GEOFENCE_CENTER_LAT + latitudeOffset;
    position.longitude = GEOFENCE_CENTER_LON;
    position.valid = true;
    return position;
}

bool noEvent(const GeofenceResult& result)
{
    return
        !result.exitEvent &&
        !result.enterEvent &&
        result.eventType == GEOFENCE_EVENT_NONE;
}

void testInvalidGPS()
{
    initGeofence();
    GPSData invalidPosition = {};

    GeofenceResult result = updateGeofence(invalidPosition);

    check("TEST 1 - etat conserve sur GPS invalide",
          result.state == GEOFENCE_UNKNOWN);
    check("TEST 1 - aucun evenement sur GPS invalide", noEvent(result));
}

void testFirstInsidePosition()
{
    initGeofence();
    GeofenceResult result = updateGeofence(positionAtDistance(300.0F));

    check("TEST 2 - premiere position classee INSIDE",
          result.state == GEOFENCE_INSIDE);
    check("TEST 2 - aucun evenement initial", noEvent(result));
}

void testSingleOutsidePosition()
{
    initGeofence();
    updateGeofence(positionAtDistance(300.0F));

    GeofenceResult result = updateGeofence(positionAtDistance(600.0F));

    check("TEST 3 - etat reste INSIDE",
          result.state == GEOFENCE_INSIDE);
    check("TEST 3 - candidat OUTSIDE 1/3",
          result.candidateState == GEOFENCE_OUTSIDE &&
              result.confirmationCount == 1);
    check("TEST 3 - aucune sortie emise", !result.exitEvent);
}

void testConfirmedExit()
{
    initGeofence();
    updateGeofence(positionAtDistance(300.0F));

    GeofenceResult first = updateGeofence(positionAtDistance(600.0F));
    GeofenceResult second = updateGeofence(positionAtDistance(610.0F));
    GeofenceResult third = updateGeofence(positionAtDistance(620.0F));

    check("TEST 4 - confirmations OUTSIDE 1/3 puis 2/3",
          first.confirmationCount == 1 &&
              second.confirmationCount == 2);
    check("TEST 4 - transition OUTSIDE a 3/3",
          third.confirmationCount == GEOFENCE_CONFIRM_COUNT &&
              third.state == GEOFENCE_OUTSIDE);
    check("TEST 4 - GEOFENCE_EXIT emis une fois", third.exitEvent);
}

void testRemainingOutside()
{
    GeofenceResult first = updateGeofence(positionAtDistance(630.0F));
    GeofenceResult second = updateGeofence(positionAtDistance(640.0F));
    GeofenceResult third = updateGeofence(positionAtDistance(650.0F));

    check("TEST 5 - etat reste OUTSIDE",
          first.state == GEOFENCE_OUTSIDE &&
              second.state == GEOFENCE_OUTSIDE &&
              third.state == GEOFENCE_OUTSIDE);
    check("TEST 5 - aucune nouvelle alerte EXIT",
          noEvent(first) && noEvent(second) && noEvent(third));
}

void testHysteresisBuffer()
{
    initGeofence();
    updateGeofence(positionAtDistance(300.0F));
    GeofenceResult fromInside =
        updateGeofence(positionAtDistance(525.0F));

    initGeofence();
    updateGeofence(positionAtDistance(600.0F));
    GeofenceResult fromOutside =
        updateGeofence(positionAtDistance(525.0F));

    check("TEST 6 - zone tampon conserve INSIDE",
          fromInside.state == GEOFENCE_INSIDE && noEvent(fromInside));
    check("TEST 6 - zone tampon conserve OUTSIDE",
          fromOutside.state == GEOFENCE_OUTSIDE && noEvent(fromOutside));
}

void testConfirmedEnter()
{
    initGeofence();
    updateGeofence(positionAtDistance(600.0F));

    GeofenceResult first = updateGeofence(positionAtDistance(490.0F));
    GeofenceResult second = updateGeofence(positionAtDistance(480.0F));
    GeofenceResult third = updateGeofence(positionAtDistance(470.0F));

    check("TEST 7 - confirmations INSIDE 1/3 puis 2/3",
          first.confirmationCount == 1 &&
              second.confirmationCount == 2);
    check("TEST 7 - transition INSIDE a 3/3",
          third.confirmationCount == GEOFENCE_CONFIRM_COUNT &&
              third.state == GEOFENCE_INSIDE);
    check("TEST 7 - GEOFENCE_ENTER emis une fois", third.enterEvent);
}

void testGPSLossDuringCandidate()
{
    initGeofence();
    updateGeofence(positionAtDistance(300.0F));
    updateGeofence(positionAtDistance(600.0F));

    GPSData invalidPosition = {};
    GeofenceResult invalidResult = updateGeofence(invalidPosition);
    GeofenceResult nextOutside =
        updateGeofence(positionAtDistance(600.0F));

    check("TEST 8 - GPS invalide conserve INSIDE",
          invalidResult.state == GEOFENCE_INSIDE &&
              noEvent(invalidResult));
    check("TEST 8 - confirmation OUTSIDE recommence a 1/3",
          nextOutside.state == GEOFENCE_INSIDE &&
              nextOutside.confirmationCount == 1 &&
              !nextOutside.exitEvent);
}

void testStaleGPSFix()
{
    initGeofence();
    updateGeofence(positionAtDistance(300.0F));

    const bool tinyGpsLocationValid = true;
    const unsigned long simulatedAge = GPS_MAX_AGE_MS + 1UL;
    GPSData stalePosition = positionAtDistance(600.0F);

    stalePosition.valid = isGPSLocationUsable(
        tinyGpsLocationValid,
        simulatedAge
    );

    GeofenceResult result = updateGeofence(stalePosition);

    check("TEST 9 - ancien FIX rejete",
          !result.positionUsable);
    check("TEST 9 - aucun changement ni evenement",
          result.state == GEOFENCE_INSIDE && noEvent(result));
}

void printSummary()
{
    Serial.println();
    Serial.println("=== RESUME GEOFENCE ===");
    Serial.print("PASS: ");
    Serial.println(counters.passed);
    Serial.print("FAIL: ");
    Serial.println(counters.failed);
    Serial.println(
        counters.failed == 0
            ? "[RESULTAT] SUCCES"
            : "[RESULTAT] ECHEC"
    );
}
} // namespace

void setup()
{
    Serial.begin(TEST_SERIAL_BAUD);
    delay(1000);

    Serial.println();
    Serial.println("=== TEST UNITAIRE GEOFENCE ===");

    testInvalidGPS();
    testFirstInsidePosition();
    testSingleOutsidePosition();
    testConfirmedExit();
    testRemainingOutside();
    testHysteresisBuffer();
    testConfirmedEnter();
    testGPSLossDuringCandidate();
    testStaleGPSFix();
    printSummary();
}

void loop()
{
    // Tests executes une seule fois dans setup().
}
