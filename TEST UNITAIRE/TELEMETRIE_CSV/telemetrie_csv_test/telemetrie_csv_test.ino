#include <Arduino.h>

// =====================================================
// TEST AUTONOME : 1 MESURE/MINUTE, 1 LOT CSV/15 MINUTES
// =====================================================

constexpr unsigned long SERIAL_BAUD_RATE = 115200UL;
constexpr size_t SAMPLES_PER_BATCH = 15;

// true  : 1 seconde represente 1 minute (lot CSV toutes les 15 secondes).
// false : cadence reelle (mesure toutes les 60 s, lot toutes les 15 min).
constexpr bool ACCELERATED_TEST = true;

constexpr unsigned long SAMPLE_INTERVAL_MS =
    ACCELERATED_TEST ? 1000UL : 60000UL;

static_assert(SAMPLES_PER_BATCH == 15,
              "Un lot doit contenir exactement 15 mesures.");

struct SimulatedTelemetry
{
    uint32_t sampleId;
    unsigned long logicalMinute;
    unsigned long capturedAtMs;

    double latitude;
    double longitude;

    float batteryVoltage;
    float batteryCurrent;
    float batteryPower;

    float solarVoltage;
    float solarCurrent;
    float solarPower;
    float solarEnergyIntervalWh;

    float acLoadVoltage;
    float acLoadCurrent;
    float acApparentPower;
    float acEnergyIntervalVAh;
};

static SimulatedTelemetry batch[SAMPLES_PER_BATCH];
static size_t sampleCount = 0;
static uint32_t nextSampleId = 1;
static uint32_t batchId = 0;
static unsigned long logicalMinute = 0;
static unsigned long lastSampleAt = 0;

static SimulatedTelemetry createSimulatedSample(unsigned long capturedAtMs)
{
    SimulatedTelemetry sample = {};

    const uint32_t phase = nextSampleId % 15U;
    const int32_t acVariation =
        static_cast<int32_t>(nextSampleId % 5U) - 2;

    sample.sampleId = nextSampleId++;
    sample.logicalMinute = logicalMinute;
    sample.capturedAtMs = capturedAtMs;

    // Leger deplacement simule pour verifier que chaque ligne est differente.
    sample.latitude =
        -4.325000 + (logicalMinute % 100UL) * 0.000010;
    sample.longitude =
        15.322000 + (logicalMinute % 100UL) * 0.000015;

    sample.batteryVoltage = 12.60F - phase * 0.015F;
    sample.batteryCurrent = 0.35F + (phase % 4U) * 0.025F;
    sample.batteryPower =
        sample.batteryVoltage * sample.batteryCurrent;

    sample.solarVoltage = 18.00F + phase * 0.080F;
    sample.solarCurrent = 1.10F + (phase % 5U) * 0.050F;
    sample.solarPower =
        sample.solarVoltage * sample.solarCurrent;
    sample.solarEnergyIntervalWh = sample.solarPower / 60.0F;

    sample.acLoadVoltage = 230.0F + acVariation * 0.50F;
    sample.acLoadCurrent = 0.60F + (phase % 3U) * 0.040F;
    sample.acApparentPower =
        sample.acLoadVoltage * sample.acLoadCurrent;
    sample.acEnergyIntervalVAh = sample.acApparentPower / 60.0F;

    return sample;
}

static void printCSVHeader()
{
    Serial.println(
        "batch_id,sample_id,logical_minute,captured_at_ms,"
        "latitude,longitude,"
        "battery_voltage_v,battery_current_a,battery_power_w,"
        "solar_voltage_v,solar_current_a,solar_power_w,"
        "solar_energy_interval_wh,"
        "ac_load_voltage_v,ac_load_current_a,"
        "ac_apparent_power_va,ac_energy_interval_vah"
    );
}

static void printCSVRow(
    const SimulatedTelemetry& sample,
    uint32_t currentBatchId
)
{
    Serial.print(currentBatchId);
    Serial.print(',');
    Serial.print(sample.sampleId);
    Serial.print(',');
    Serial.print(sample.logicalMinute);
    Serial.print(',');
    Serial.print(sample.capturedAtMs);
    Serial.print(',');

    Serial.print(sample.latitude, 6);
    Serial.print(',');
    Serial.print(sample.longitude, 6);
    Serial.print(',');

    Serial.print(sample.batteryVoltage, 3);
    Serial.print(',');
    Serial.print(sample.batteryCurrent, 3);
    Serial.print(',');
    Serial.print(sample.batteryPower, 3);
    Serial.print(',');

    Serial.print(sample.solarVoltage, 3);
    Serial.print(',');
    Serial.print(sample.solarCurrent, 3);
    Serial.print(',');
    Serial.print(sample.solarPower, 3);
    Serial.print(',');
    Serial.print(sample.solarEnergyIntervalWh, 4);
    Serial.print(',');

    Serial.print(sample.acLoadVoltage, 2);
    Serial.print(',');
    Serial.print(sample.acLoadCurrent, 3);
    Serial.print(',');
    Serial.print(sample.acApparentPower, 3);
    Serial.print(',');
    Serial.println(sample.acEnergyIntervalVAh, 4);
}

static bool sendBatchAsCSV()
{
    if (sampleCount != SAMPLES_PER_BATCH)
    {
        Serial.println(
            "[ERREUR] Envoi refuse : le lot ne contient pas 15 mesures."
        );
        return false;
    }

    const uint32_t currentBatchId = ++batchId;

    Serial.println();
    Serial.print("[ENVOI] Lot CSV ");
    Serial.print(currentBatchId);
    Serial.print(" - ");
    Serial.print(sampleCount);
    Serial.println(" mesures");
    Serial.println("CSV_BEGIN");

    printCSVHeader();

    for (size_t index = 0; index < sampleCount; index++)
    {
        printCSVRow(batch[index], currentBatchId);
    }

    Serial.println("CSV_END");
    Serial.println("[ENVOI] Lot CSV termine.");
    Serial.println();

    // Le moniteur serie represente ici un transport ayant accepte le lot.
    return true;
}

static void collectSample(unsigned long capturedAtMs)
{
    if (sampleCount >= SAMPLES_PER_BATCH)
    {
        Serial.println(
            "[ERREUR] Memoire du lot pleine : mesure non ajoutee."
        );
        return;
    }

    batch[sampleCount] = createSimulatedSample(capturedAtMs);
    sampleCount++;

    Serial.print("[PRELEVEMENT] Mesure ");
    Serial.print(sampleCount);
    Serial.print('/');
    Serial.print(SAMPLES_PER_BATCH);
    Serial.print(" | minute logique ");
    Serial.print(logicalMinute);
    Serial.print(" | batterie ");
    Serial.print(batch[sampleCount - 1].batteryVoltage, 2);
    Serial.println(" V");
}

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    Serial.println();
    Serial.println("=== TEST TELEMETRIE GROUPEE CSV ===");
    Serial.println("15 mesures sont conservees avant chaque envoi.");

    if (ACCELERATED_TEST)
    {
        Serial.println(
            "[MODE] Accelere : 1 seconde = 1 minute logique."
        );
        Serial.println("[MODE] Un lot CSV sera emis toutes les 15 secondes.");
    }
    else
    {
        Serial.println("[MODE] Reel : une mesure toutes les 60 secondes.");
        Serial.println("[MODE] Un lot CSV sera emis toutes les 15 minutes.");
    }

    Serial.println("[ATTENTE] Premier prelevement au prochain intervalle.");

    lastSampleAt = millis();
}

void loop()
{
    const unsigned long now = millis();
    const unsigned long elapsed = now - lastSampleAt;

    if (elapsed < SAMPLE_INTERVAL_MS)
    {
        return;
    }

    // Saute proprement les intervalles manques sans inventer de mesures.
    const unsigned long intervalsElapsed =
        elapsed / SAMPLE_INTERVAL_MS;

    lastSampleAt += intervalsElapsed * SAMPLE_INTERVAL_MS;
    logicalMinute += intervalsElapsed;

    if (intervalsElapsed > 1UL)
    {
        Serial.print("[AVERTISSEMENT] Intervalles sans mesure : ");
        Serial.println(intervalsElapsed - 1UL);
    }

    collectSample(now);

    if (sampleCount == SAMPLES_PER_BATCH && sendBatchAsCSV())
    {
        // Le lot n'est libere qu'apres la reussite de l'envoi simule.
        sampleCount = 0;
    }
}
