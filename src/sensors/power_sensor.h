#ifndef POWER_SENSOR_H
#define POWER_SENSOR_H

#include <Arduino.h>

// =====================================================
// IOT BOX - INTERFACE CAPTEUR INA219
// =====================================================

// Structure contenant une mesure electrique complete
struct PowerData {
    float voltage;   // Tension en volts
    float current;   // Courant en amperes
    float power;     // Puissance en watts
    bool valid;      // true si la mesure est valide
};

// Initialise le capteur INA219.
// Retourne true si le capteur est detecte.
bool initPowerSensor();

// Lit les donnees du INA219.
// Retourne une structure PowerData.
PowerData readPowerSensor();

// Indique si le INA219 a ete correctement initialise.
bool isPowerSensorAvailable();

#endif