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
// Age maximal d'une position utilisable par la telemetrie.
#define GPS_MAX_AGE_MS 5000UL

//  HORLOGE RTC DS1302
// Le DS1302 conserve l'heure locale correspondant a ce fuseau fixe.
#define RTC_GMT_OFFSET_MINUTES 60
#define RTC_TIMEZONE_LABEL "GMT+1"
#define RTC_AUTO_INITIALIZE_IF_INVALID 1

// Wi-Fi
#define WIFI_SSID "ODC_LOCAL"
// Two literal backslashes in the password are represented by four in this C++ string.
#define WIFI_PASSWORD "SecureTopTop=ABC_Puits"

//BACKEND REEL
#define BACKEND_URL "http://10.252.252.40:5000/api/iot/telemetry"
#define IOT_API_KEY "djua"
#define ENABLE_HTTP_BACKEND 1

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
