#include <Arduino.h>
#include <TinyGPSPlus.h>

// =====================================================
// CONFIGURATION AUTONOME DU TEST NEO-6M
// =====================================================

constexpr uint8_t NEO6M_TEST_RX_PIN = 16; // ESP32 RX <- TX du NEO-6M
constexpr uint8_t NEO6M_TEST_TX_PIN = 17; // ESP32 TX -> RX du NEO-6M (optionnel)

constexpr unsigned long NEO6M_TEST_SERIAL_BAUD = 115200UL;
constexpr unsigned long NEO6M_TEST_GPS_BAUD = 9600UL;
constexpr unsigned long NEO6M_TEST_DURATION_MS = 60000UL;
constexpr unsigned long NEO6M_PROGRESS_INTERVAL_MS = 5000UL;
constexpr unsigned long NEO6M_MINIMUM_CHARACTERS = 50UL;

// 1 : recopie les trames NMEA brutes dans le moniteur serie.
// 0 : affiche uniquement l'avancement et le rapport de test.
#define NEO6M_ECHO_NMEA 0

// Sans vue degagee du ciel, l'absence de FIX n'indique pas forcement une panne.
// Mettre a 1 pour rendre le FIX obligatoire pendant la duree du test.
#define NEO6M_REQUIRE_FIX 0

static_assert(NEO6M_TEST_RX_PIN != NEO6M_TEST_TX_PIN,
              "Les broches RX et TX doivent etre differentes.");

static HardwareSerial neo6mSerial(2);
static TinyGPSPlus gps;

struct TestCounters
{
    uint16_t passed;
    uint16_t failed;
    uint16_t skipped;
};

static TestCounters counters = {0, 0, 0};
static unsigned long receivedBytes = 0;

static void reportPass(const char* testName)
{
    counters.passed++;
    Serial.print("[PASS] ");
    Serial.println(testName);
}

static void reportFail(const char* testName, const char* reason)
{
    counters.failed++;
    Serial.print("[FAIL] ");
    Serial.print(testName);
    Serial.print(" - ");
    Serial.println(reason);
}

static void reportSkip(const char* testName, const char* reason)
{
    counters.skipped++;
    Serial.print("[SKIP] ");
    Serial.print(testName);
    Serial.print(" - ");
    Serial.println(reason);
}

static void check(
    const char* testName,
    bool condition,
    const char* failureReason
)
{
    if (condition)
    {
        reportPass(testName);
    }
    else
    {
        reportFail(testName, failureReason);
    }
}

static void printProgress(unsigned long elapsedMs)
{
    Serial.print("[MESURE] ");
    Serial.print(elapsedMs / 1000UL);
    Serial.print(" s | caracteres: ");
    Serial.print(gps.charsProcessed());
    Serial.print(" | checksums OK: ");
    Serial.print(gps.passedChecksum());
    Serial.print(" | checksums KO: ");
    Serial.print(gps.failedChecksum());
    Serial.print(" | FIX: ");
    Serial.println(gps.location.isValid() ? "OUI" : "NON");
}

static void acquireGPSData()
{
    const unsigned long startedAt = millis();
    unsigned long lastProgressAt = startedAt;

    while (millis() - startedAt < NEO6M_TEST_DURATION_MS)
    {
        while (neo6mSerial.available() > 0)
        {
            const char character =
                static_cast<char>(neo6mSerial.read());

            receivedBytes++;
            gps.encode(character);

#if NEO6M_ECHO_NMEA
            Serial.write(character);
#endif
        }

        const unsigned long now = millis();

        if (now - lastProgressAt >= NEO6M_PROGRESS_INTERVAL_MS)
        {
#if NEO6M_ECHO_NMEA
            Serial.println();
#endif
            printProgress(now - startedAt);
            lastProgressAt = now;
        }

        // Laisse le planificateur ESP32 travailler sans ralentir l'UART GPS.
        delay(1);
    }

    // Traite les derniers octets deja presents dans le tampon UART.
    while (neo6mSerial.available() > 0)
    {
        const char character =
            static_cast<char>(neo6mSerial.read());

        receivedBytes++;
        gps.encode(character);

#if NEO6M_ECHO_NMEA
        Serial.write(character);
#endif
    }
}

static void testCommunication()
{
    check(
        "Reception de donnees sur UART2",
        receivedBytes > 0,
        "Aucun octet recu : verifier alimentation, masse, TX du GPS et GPIO 16"
    );

    if (receivedBytes == 0)
    {
        reportSkip(
            "Flux NMEA suffisamment long",
            "Aucune donnee serie disponible"
        );
        reportSkip(
            "Au moins une trame NMEA valide",
            "Aucune donnee serie disponible"
        );
        reportSkip(
            "Integrite des checksums NMEA",
            "Aucune trame NMEA disponible"
        );
        return;
    }

    check(
        "Flux NMEA suffisamment long",
        gps.charsProcessed() >= NEO6M_MINIMUM_CHARACTERS,
        "Trop peu de caracteres : verifier le debit GPS (9600 bauds)"
    );

    check(
        "Au moins une trame NMEA valide",
        gps.passedChecksum() > 0,
        "Aucun checksum valide : verifier le debit et la qualite du cablage"
    );

    if (gps.passedChecksum() == 0)
    {
        reportSkip(
            "Integrite des checksums NMEA",
            "Aucune trame valide ne permet la comparaison"
        );
    }
    else
    {
        check(
            "Integrite des checksums NMEA",
            gps.failedChecksum() == 0,
            "Une ou plusieurs trames sont corrompues"
        );
    }
}

static void testNavigationData()
{
    if (!gps.location.isValid())
    {
#if NEO6M_REQUIRE_FIX
        reportFail(
            "Obtention d'un FIX GPS",
            "Aucun FIX pendant le delai : placer l'antenne sous un ciel degage"
        );
#else
        reportSkip(
            "Obtention d'un FIX GPS",
            "Aucun FIX; ce controle est optionnel dans la configuration actuelle"
        );
#endif
        reportSkip(
            "Coordonnees dans les plages attendues",
            "Aucune position valide disponible"
        );
    }
    else
    {
        reportPass("Obtention d'un FIX GPS");

        const double latitude = gps.location.lat();
        const double longitude = gps.location.lng();

        Serial.print("Latitude  : ");
        Serial.println(latitude, 6);
        Serial.print("Longitude : ");
        Serial.println(longitude, 6);

        check(
            "Coordonnees dans les plages attendues",
            latitude >= -90.0 && latitude <= 90.0 &&
                longitude >= -180.0 && longitude <= 180.0,
            "Latitude ou longitude hors plage"
        );
    }

    if (gps.satellites.isValid())
    {
        Serial.print("Satellites : ");
        Serial.println(gps.satellites.value());

        check(
            "Nombre de satellites coherent",
            gps.satellites.value() <= 64,
            "Valeur de satellites incoherente"
        );
    }
    else
    {
        reportSkip(
            "Nombre de satellites coherent",
            "Champ satellites non recu ou invalide"
        );
    }

    if (gps.date.isValid() && gps.time.isValid())
    {
        reportPass("Date et heure GPS valides");
    }
    else
    {
        reportSkip(
            "Date et heure GPS valides",
            "Champ RMC/GGA non recu ou invalide"
        );
    }
}

static void printDiagnostics()
{
    Serial.println();
    Serial.println("=== DIAGNOSTIC NMEA ===");
    Serial.print("Octets UART recus       : ");
    Serial.println(receivedBytes);
    Serial.print("Caracteres traites      : ");
    Serial.println(gps.charsProcessed());
    Serial.print("Trames checksum valide  : ");
    Serial.println(gps.passedChecksum());
    Serial.print("Trames checksum invalide: ");
    Serial.println(gps.failedChecksum());
    Serial.print("Trames avec donnees FIX : ");
    Serial.println(gps.sentencesWithFix());
}

static void printSummary()
{
    Serial.println();
    Serial.println("=== RESUME ===");

    Serial.print("PASS: ");
    Serial.println(counters.passed);

    Serial.print("FAIL: ");
    Serial.println(counters.failed);

    Serial.print("SKIP: ");
    Serial.println(counters.skipped);

    Serial.println(
        counters.failed == 0
            ? "[RESULTAT] SUCCES"
            : "[RESULTAT] ECHEC"
    );
}

void setup()
{
    Serial.begin(NEO6M_TEST_SERIAL_BAUD);
    delay(1000);

    Serial.println();
    Serial.println("=== TEST MODULE GPS NEO-6M ===");
    Serial.print("RX ESP32 GPIO : ");
    Serial.println(NEO6M_TEST_RX_PIN);
    Serial.print("TX ESP32 GPIO : ");
    Serial.println(NEO6M_TEST_TX_PIN);
    Serial.print("Debit GPS      : ");
    Serial.println(NEO6M_TEST_GPS_BAUD);
    Serial.print("Duree du test  : ");
    Serial.print(NEO6M_TEST_DURATION_MS / 1000UL);
    Serial.println(" s");
    Serial.println();

    neo6mSerial.begin(
        NEO6M_TEST_GPS_BAUD,
        SERIAL_8N1,
        NEO6M_TEST_RX_PIN,
        NEO6M_TEST_TX_PIN
    );

    Serial.println("[TEST] Acquisition des donnees GPS...");
    acquireGPSData();

    Serial.println();
    Serial.println("=== CONTROLES ===");
    testCommunication();
    testNavigationData();
    printDiagnostics();
    printSummary();
}

void loop()
{
    // La campagne est executee une seule fois dans setup().
}
