#ifndef CONFIG_H
#define CONFIG_H

// IOT BOX - CONFIGURATION GENERALE
//  IDENTIFICATION BACKEND 
#define DEVICE_ID "DJUA-KIN-000001"

//  MONITEUR SERIE 
#define SERIAL_BAUD_RATE 115200

//  INA219 BATTERIE 
#define INA219_I2C_ADDRESS 0x40

//  GPS NEO-6M 
#define GPS_BAUD_RATE 9600

// Wi-Fi
#define WIFI_SSID "ODC_LOCAL"
// Two literal backslashes in the password are represented by four in this C++ string.
#define WIFI_PASSWORD "SecureTopTop=ABC_Puits"

//BACKEND REEL
#define BACKEND_URL "http://10.255.209.155:5000/api/iot/telemetry"
#define IOT_API_KEY "djua"
#define ENABLE_HTTP_BACKEND 0

// MQTT TESTING
// Public broker for short-term integration tests only. Do not use this broker
// or these public topics for production or sensitive telemetry.
#define MQTT_BROKER_HOST "test.mosquitto.org"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC_PREFIX "djua/test"
#define MQTT_RETRY_INTERVAL_MS 10000UL

//TELEMETrie
#define TELEMETRY_INTERVAL_MS 10000UL
#define WIFI_RETRY_INTERVAL_MS 10000UL

#endif
