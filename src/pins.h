#ifndef PINS_H
#define PINS_H

// =====================================================
// IOT BOX - BROCHES ESP32
// =====================================================

// ---------- INA219 / I2C ----------
#define I2C_SDA_PIN 33
#define I2C_SCL_PIN 14

// ---------- GPS NEO-6M / UART2 ----------

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

// ---------- RTC DS1302 / THREE-WIRE ----------

#define DS1302_DATA_PIN 25
#define DS1302_CLOCK_PIN 26
#define DS1302_CE_PIN 27

// ---------- FUTUR SIM800C ----------

// #define GSM_RX_PIN 
// #define GSM_TX_PIN 

// ---------- FUTURE MICROSD ----------
// #define SD_CS_PIN 

#endif
