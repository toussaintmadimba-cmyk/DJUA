#include "power_sensor.h"

#include <Wire.h>
#include <Adafruit_INA219.h>

#include "../pins.h"
#include "../config.h"

// =====================================================
// IOT BOX - PILOTE INA219
// =====================================================

// Creation de l'objet INA219 avec l'adresse definie
// dans config.h.
Adafruit_INA219 ina219(INA219_I2C_ADDRESS);

// Etat du capteur.
static bool powerSensorAvailable = false;


// =====================================================
// INITIALISATION
// =====================================================

bool initPowerSensor()
{
    // Initialisation du bus I2C de l'ESP32.
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // Tentative de communication avec le INA219.
    if (!ina219.begin()) {
        powerSensorAvailable = false;
        return false;
    }

    /*
     * Calibration standard de la bibliotheque Adafruit.
     *
     * Convient pour notre premier test.
     * Nous ajusterons la calibration plus tard si necessaire
     * selon les tensions et courants reels du systeme.
     */
    ina219.setCalibration_32V_2A();

    powerSensorAvailable = true;

    return true;
}


// =====================================================
// LECTURE DU CAPTEUR
// =====================================================

PowerData readPowerSensor()
{
    PowerData data;

    // Valeurs par defaut en cas d'erreur.
    data.voltage = 0.0;
    data.current = 0.0;
    data.power = 0.0;
    data.valid = false;

    // Ne pas tenter de lire si le capteur
    // n'a pas ete initialise.
    if (!powerSensorAvailable) {
        return data;
    }

    // -------------------------------------------------
    // Lecture INA219
    // -------------------------------------------------

    // Tension mesuree sur le shunt.
    float shuntVoltage_mV = ina219.getShuntVoltage_mV();

    // Tension du bus.
    float busVoltage_V = ina219.getBusVoltage_V();

    // Courant retourne par la bibliotheque en mA.
    float current_mA = ina219.getCurrent_mA();

    // -------------------------------------------------
    // Calcul de la tension totale
    // -------------------------------------------------
    //
    // Le INA219 mesure :
    //
    // busVoltage + shuntVoltage
    //
    // On convertit donc mV -> V.
    //

    float loadVoltage_V =
        busVoltage_V + (shuntVoltage_mV / 1000.0);

    // Conversion mA -> A.
    float current_A = current_mA / 1000.0;

    // Calcul de la puissance.
    float power_W = loadVoltage_V * current_A;

    // -------------------------------------------------
    // Remplissage de la structure PowerData
    // -------------------------------------------------

    data.voltage = loadVoltage_V;
    data.current = current_A;
    data.power = power_W;
    data.valid = true;

    return data;
}


// =====================================================
// ETAT DU CAPTEUR
// =====================================================

bool isPowerSensorAvailable()
{
    return powerSensorAvailable;
}