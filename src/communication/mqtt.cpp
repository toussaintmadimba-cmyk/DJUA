#include "mqtt.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "../config.h"

static WiFiClient mqttWiFiClient;
static PubSubClient mqttClient(mqttWiFiClient);
static unsigned long lastMQTTRetry = 0;

static String telemetryTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/telemetry";
}

static String statusTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/status";
}

static bool connectMQTT()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    Serial.print("[MQTT] Connexion a ");
    Serial.println(MQTT_BROKER_HOST);

    String clientId = String("djua-")  + DEVICE_ID;
    String willTopic = statusTopic();

    if (!mqttClient.connect(clientId.c_str(), willTopic.c_str(), 0, true, "offline"))
    {
        Serial.print("[MQTT] Echec, code : ");
        Serial.println(mqttClient.state());
        return false;
    }

    mqttClient.publish(willTopic.c_str(), "online", true);
    Serial.println("[MQTT] CONNECTE");
    Serial.print("[MQTT] Topic telemetry : ");
    Serial.println(telemetryTopic());
    return true;
}

void initMQTT()
{
    mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    // The default PubSubClient packet limit is too small for the full payload.
    mqttClient.setBufferSize(1024);
}

void updateMQTT()
{
    if (!mqttClient.connected())
    {
        unsigned long now = millis();
        if (now - lastMQTTRetry >= MQTT_RETRY_INTERVAL_MS)
        {
            lastMQTTRetry = now;
            connectMQTT();
        }
        return;
    }

    mqttClient.loop();
}

bool isMQTTConnected()
{
    return mqttClient.connected();
}

bool sendTelemetryToMQTT(const TelemetryData& data)
{
    if (!mqttClient.connected())
    {
        Serial.println("[MQTT] Impossible : non connecte.");
        return false;
    }

    StaticJsonDocument<1024> doc;
    doc["kit_id"] = DEVICE_ID;
    doc["timestamp_ms"] = millis();
    doc["interval_seconds"] = data.intervalSeconds;
    doc["latitude"] = data.latitude;
    doc["longitude"] = data.longitude;

    JsonObject battery = doc.createNestedObject("battery");
    battery["voltage_v"] = data.batteryVoltage;
    battery["current_a"] = data.batteryCurrent;
    battery["power_w"] = data.batteryPower;

    JsonObject solar = doc.createNestedObject("solar");
    solar["voltage_v"] = data.solarVoltage;
    solar["current_a"] = data.solarCurrent;
    solar["power_w"] = data.solarPower;
    solar["energy_interval_wh"] = data.solarEnergyIntervalWh;

    JsonObject dcLoad = doc.createNestedObject("dc_load");
    dcLoad["voltage_v"] = data.dcLoadVoltage;
    dcLoad["current_a"] = data.dcLoadCurrent;
    dcLoad["power_w"] = data.dcLoadPower;
    dcLoad["energy_interval_wh"] = data.dcLoadEnergyIntervalWh;

    JsonObject acLoad = doc.createNestedObject("ac_load");
    acLoad["voltage_v"] = data.acLoadVoltage;
    acLoad["current_a"] = data.acLoadCurrent;
    acLoad["apparent_power_va"] = data.acApparentPower;
    acLoad["energy_interval_vah"] = data.acEnergyIntervalVAh;

    char payload[1024];
    size_t payloadSize = serializeJson(doc, payload, sizeof(payload));
    if (payloadSize == 0 || payloadSize >= sizeof(payload))
    {
        Serial.println("[MQTT] Erreur de construction JSON.");
        return false;
    }

    String topic = telemetryTopic();
    bool sent = mqttClient.publish(topic.c_str(), payload, false);
    Serial.println(sent ? "[MQTT] TELEMETRIE ENVOYEE" : "[MQTT] ECHEC ENVOI");
    return sent;
}
