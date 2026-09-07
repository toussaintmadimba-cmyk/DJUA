#include <Arduino.h>
#include <RtcDS1302.h>

// =====================================================
// CONFIGURATION AUTONOME DU TEST DS1302
// =====================================================

constexpr uint8_t DS1302_TEST_CLOCK_PIN = 26; // CLK / SCLK
constexpr uint8_t DS1302_TEST_DATA_PIN = 25;  // DAT / IO
constexpr uint8_t DS1302_TEST_CE_PIN = 27;    // RST / CE

constexpr unsigned long DS1302_TEST_SERIAL_BAUD = 115200UL;
constexpr unsigned long DS1302_ADVANCE_WAIT_MS = 2200UL;

// Initialise automatiquement une date invalide avec la date de compilation.
// Une horloge deja valide n'est jamais remise a l'heure par cette option.
#define DS1302_INITIALIZE_IF_INVALID 1

// 0 : pas de test destructif apres l'initialisation eventuelle ci-dessus.
// 1 : teste les ecritures puis restaure l'etat initial si possible.
#define DS1302_ENABLE_WRITE_TESTS 0

static_assert(DS1302_TEST_DATA_PIN != DS1302_TEST_CLOCK_PIN,
              "Les broches DATA et CLOCK doivent etre differentes.");
static_assert(DS1302_TEST_DATA_PIN != DS1302_TEST_CE_PIN,
              "Les broches DATA et CE doivent etre differentes.");
static_assert(DS1302_TEST_CLOCK_PIN != DS1302_TEST_CE_PIN,
              "Les broches CLOCK et CE doivent etre differentes.");

// L'ordre impose par ThreeWire est IO, SCLK, CE.
static ThreeWire ds1302Wire(
    DS1302_TEST_DATA_PIN,
    DS1302_TEST_CLOCK_PIN,
    DS1302_TEST_CE_PIN
);
static RtcDS1302<ThreeWire> rtc(ds1302Wire);

struct TestCounters
{
    uint16_t passed;
    uint16_t failed;
    uint16_t skipped;
};

static TestCounters counters = {0, 0, 0};

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

static void printDateTime(const RtcDateTime& dateTime)
{
    char buffer[24];

    snprintf(
        buffer,
        sizeof(buffer),
        "%04u-%02u-%02u %02u:%02u:%02u",
        dateTime.Year(),
        dateTime.Month(),
        dateTime.Day(),
        dateTime.Hour(),
        dateTime.Minute(),
        dateTime.Second()
    );

    Serial.println(buffer);
}

static bool fieldsAreInExpectedRanges(const RtcDateTime& dateTime)
{
    return
        dateTime.Year() >= 2000 &&
        dateTime.Year() <= 2099 &&
        dateTime.Month() >= 1 &&
        dateTime.Month() <= 12 &&
        dateTime.Day() >= 1 &&
        dateTime.Day() <= 31 &&
        dateTime.Hour() <= 23 &&
        dateTime.Minute() <= 59 &&
        dateTime.Second() <= 59;
}

static bool sameCalendarSecond(
    const RtcDateTime& expected,
    const RtcDateTime& actual
)
{
    if (!expected.IsValid() || !actual.IsValid())
    {
        return false;
    }

    uint32_t expectedSeconds = expected.TotalSeconds();
    uint32_t actualSeconds = actual.TotalSeconds();

    return
        actualSeconds >= expectedSeconds &&
        actualSeconds - expectedSeconds <= 2;
}

static void initializeClockIfRequired()
{
#if DS1302_INITIALIZE_IF_INVALID
    RtcDateTime currentDateTime = rtc.GetDateTime();
    bool dateTimeValid =
        rtc.IsDateTimeValid() &&
        currentDateTime.IsValid();
    bool running = rtc.GetIsRunning();

    if (dateTimeValid && running)
    {
        Serial.println(
            "[INIT] Horloge deja valide et active : aucune ecriture."
        );
        return;
    }

    bool originalWriteProtection = rtc.GetIsWriteProtected();

    Serial.println("[INIT] Correction de l'etat du DS1302...");
    rtc.SetIsWriteProtected(false);

    if (!running)
    {
        Serial.println("[INIT] Demarrage de l'oscillateur.");
        rtc.SetIsRunning(true);
    }

    if (!dateTimeValid)
    {
        RtcDateTime compileDateTime(__DATE__, __TIME__);

        Serial.print("[INIT] Date de compilation appliquee : ");
        printDateTime(compileDateTime);
        rtc.SetDateTime(compileDateTime);
    }

    rtc.SetIsWriteProtected(originalWriteProtection);
    delay(250);

    RtcDateTime initializedDateTime = rtc.GetDateTime();
    bool initializationSucceeded =
        initializedDateTime.IsValid() &&
        rtc.IsDateTimeValid() &&
        rtc.GetIsRunning();

    Serial.print("[INIT] Date/heure apres correction : ");
    printDateTime(initializedDateTime);
    Serial.println(
        initializationSucceeded
            ? "[INIT] Initialisation reussie."
            : "[INIT] Initialisation impossible : verifier le cablage et la pile."
    );
#else
    Serial.println(
        "[INIT] Initialisation automatique desactivee dans ds1302_test.ino."
    );
#endif
}

static void testReadOnlyClock()
{
    RtcDateTime firstRead = rtc.GetDateTime();
    bool dateTimeValid = rtc.IsDateTimeValid() && firstRead.IsValid();

    Serial.print("Date/heure lue : ");
    printDateTime(firstRead);

    check(
        "Date/heure valide",
        dateTimeValid,
        "Verifier le cablage, la pile et l'initialisation de l'horloge"
    );

    check(
        "Champs calendaires dans les plages attendues",
        fieldsAreInExpectedRanges(firstRead),
        "Une ou plusieurs valeurs sont hors plage"
    );

    bool running = rtc.GetIsRunning();
    check(
        "Oscillateur actif",
        running,
        "Le bit Clock Halt indique que l'horloge est arretee"
    );

    if (!dateTimeValid || !running)
    {
        reportSkip(
            "Progression de l'horloge",
            "Date invalide ou oscillateur arrete"
        );
        return;
    }

    delay(DS1302_ADVANCE_WAIT_MS);

    RtcDateTime secondRead = rtc.GetDateTime();
    uint32_t firstSeconds = firstRead.TotalSeconds();
    uint32_t secondSeconds = secondRead.TotalSeconds();

    bool progressed =
        secondRead.IsValid() &&
        secondSeconds > firstSeconds &&
        secondSeconds - firstSeconds >= 1 &&
        secondSeconds - firstSeconds <= 4;

    check(
        "Progression de l'horloge",
        progressed,
        "L'heure n'a pas progresse dans la fenetre attendue"
    );
}

#if DS1302_ENABLE_WRITE_TESTS
static void testWriteAndRestore()
{
    RtcDateTime originalDateTime = rtc.GetDateTime();

    if (!originalDateTime.IsValid())
    {
        reportSkip(
            "Ecriture et restauration de la date",
            "La date initiale est invalide et ne peut pas etre restauree"
        );
        reportSkip(
            "Ecriture et restauration de la RAM",
            "Test groupe ignore pour conserver un etat sur"
        );
        return;
    }

    bool originalWriteProtection = rtc.GetIsWriteProtected();
    bool originalRunningState = rtc.GetIsRunning();
    uint8_t originalMemoryByte = rtc.GetMemory(0);
    unsigned long writeTestStartedAt = millis();

    rtc.SetIsWriteProtected(false);
    check(
        "Desactivation temporaire de la protection en ecriture",
        !rtc.GetIsWriteProtected(),
        "La protection en ecriture est restee active"
    );

    rtc.SetIsRunning(true);

    const RtcDateTime testDateTime(2024, 2, 29, 12, 34, 56);
    rtc.SetDateTime(testDateTime);
    delay(1100);

    RtcDateTime dateTimeReadBack = rtc.GetDateTime();
    check(
        "Ecriture et relecture d'une date connue",
        sameCalendarSecond(testDateTime, dateTimeReadBack),
        "La date relue ne correspond pas a la date ecrite"
    );

    const uint8_t testPattern = 0xA5;
    rtc.SetMemory(0, testPattern);
    check(
        "Ecriture et relecture de la RAM",
        rtc.GetMemory(0) == testPattern,
        "L'octet relu ne correspond pas au motif 0xA5"
    );

    unsigned long elapsedSeconds =
        (millis() - writeTestStartedAt) / 1000UL;
    RtcDateTime restoredDateTime(
        originalDateTime.TotalSeconds() + elapsedSeconds
    );

    rtc.SetDateTime(restoredDateTime);
    rtc.SetMemory(0, originalMemoryByte);
    rtc.SetIsRunning(originalRunningState);
    rtc.SetIsWriteProtected(originalWriteProtection);

    RtcDateTime restoredReadBack = rtc.GetDateTime();
    check(
        "Restauration de la date initiale",
        sameCalendarSecond(restoredDateTime, restoredReadBack),
        "La date initiale n'a pas ete restauree"
    );

    check(
        "Restauration de la RAM",
        rtc.GetMemory(0) == originalMemoryByte,
        "L'octet RAM initial n'a pas ete restaure"
    );

    check(
        "Restauration de l'etat de l'oscillateur",
        rtc.GetIsRunning() == originalRunningState,
        "L'etat initial de l'oscillateur n'a pas ete restaure"
    );

    check(
        "Restauration de la protection en ecriture",
        rtc.GetIsWriteProtected() == originalWriteProtection,
        "L'etat initial de protection n'a pas ete restaure"
    );
}
#endif

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
    Serial.begin(DS1302_TEST_SERIAL_BAUD);
    delay(1000);

    Serial.println();
    Serial.println("=== TEST MODULE DS1302 ===");
    Serial.print("DATA/IO GPIO : ");
    Serial.println(DS1302_TEST_DATA_PIN);
    Serial.print("CLOCK GPIO   : ");
    Serial.println(DS1302_TEST_CLOCK_PIN);
    Serial.print("CE/RST GPIO  : ");
    Serial.println(DS1302_TEST_CE_PIN);
    Serial.println();

    rtc.Begin();
    initializeClockIfRequired();
    Serial.println();

    testReadOnlyClock();

#if DS1302_ENABLE_WRITE_TESTS
    testWriteAndRestore();
#else
    reportSkip(
        "Tests d'ecriture",
        "Desactives dans ds1302_test.ino"
    );
#endif

    printSummary();
}

void loop()
{
    // Les tests sont executes une seule fois dans setup().
}
