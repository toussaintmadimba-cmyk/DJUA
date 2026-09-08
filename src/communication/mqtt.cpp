#include "mqtt.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "../config.h"

static WiFiClient mqttWiFiClient;
static PubSubClient mqttClient(mqttWiFiClient);
static unsigned long lastMQTTRetry = 0;
static char pendingGeofenceEventPayload[768] = {};
static bool hasPendingGeofenceEvent = false;

static String telemetryTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/telemetry";
}

static String statusTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/status";
}

static String geofenceTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/geofence";
}

static String geofenceEventsTopic()
{
    return geofenceTopic() + "/events";
}

static bool publishPendingGeofenceEvent()
{
    if (!hasPendingGeofenceEvent || !mqttClient.connected())
    {
        return !hasPendingGeofenceEvent;
    }

    const String topic = geofenceEventsTopic();
    const bool sent = mqttClient.publish(
        topic.c_str(),
        pendingGeofenceEventPayload,
        false
    );

    if (sent)
    {
        hasPendingGeofenceEvent = false;
        pendingGeofenceEventPayload[0] = '\0';
        Serial.println("[MQTT] EVENEMENT GEOFENCE EN ATTENTE ENVOYE");
    }

    return sent;
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
    Serial.print("[MQTT] Topic geofence : ");
    Serial.println(geofenceTopic());
    publishPendingGeofenceEvent();
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
    publishPendingGeofenceEvent();
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
    // Conserve timestamp_ms pour la compatibilite avec les consommateurs actuels.
    doc["timestamp_ms"] = millis();

    if (data.timestampValid)
    {
        doc["timestamp"] = data.timestamp;
        doc["timezone"] = RTC_TIMEZONE_LABEL;
    }

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

bool sendGeofenceToMQTT(
    const GeofenceResult& result,
    const char* timestamp,
    bool timestampValid
)
{
    StaticJsonDocument<768> doc;
    doc["kit_id"] = result.deviceId;
    // Temps ecoule depuis le demarrage, conserve meme si le RTC est invalide.
    doc["timestamp_ms"] = millis();

    if (timestampValid && timestamp != nullptr && timestamp[0] != '\0')
    {
        doc["timestamp"] = timestamp;
        doc["timezone"] = RTC_TIMEZONE_LABEL;
    }

    doc["state"] = geofenceStateToString(result.state);
    doc["event"] = geofenceEventToString(result.eventType);
    doc["position_usable"] = result.positionUsable;

    if (result.positionUsable)
    {
        doc["latitude"] = result.latitude;
        doc["longitude"] = result.longitude;
        doc["distance_m"] = result.distanceMeters;
    }
    else
    {
        doc["latitude"] = nullptr;
        doc["longitude"] = nullptr;
        doc["distance_m"] = nullptr;
    }

    JsonObject zone = doc.createNestedObject("zone");
    zone["center_latitude"] = GEOFENCE_CENTER_LAT;
    zone["center_longitude"] = GEOFENCE_CENTER_LON;
    zone["enter_radius_m"] = GEOFENCE_ENTER_RADIUS_M;
    zone["exit_radius_m"] = GEOFENCE_EXIT_RADIUS_M;

    JsonObject confirmation = doc.createNestedObject("confirmation");
    confirmation["candidate_state"] =
        geofenceStateToString(result.candidateState);
    confirmation["count"] = result.confirmationCount;
    confirmation["required"] = GEOFENCE_CONFIRM_COUNT;

    char payload[768];
    const size_t payloadSize = serializeJson(doc, payload, sizeof(payload));
    if (payloadSize == 0 || payloadSize >= sizeof(payload))
    {
        Serial.println("[MQTT] Erreur de construction JSON geofence.");
        return false;
    }

    const bool isEvent = result.eventType != GEOFENCE_EVENT_NONE;

    if (!mqttClient.connected())
    {
        if (isEvent)
        {
            strncpy(
                pendingGeofenceEventPayload,
                payload,
                sizeof(pendingGeofenceEventPayload) - 1
            );
            pendingGeofenceEventPayload[
                sizeof(pendingGeofenceEventPayload) - 1
            ] = '\0';
            hasPendingGeofenceEvent = true;
            Serial.println("[MQTT] Evenement geofence garde pour reconnexion.");
        }

        Serial.println("[MQTT] Geofence non envoye : non connecte.");
        return false;
    }

    const String currentGeofenceTopic = geofenceTopic();
    // L'etat est retenu afin qu'un observateur retrouve le dernier controle.
    const bool statusSent = mqttClient.publish(
        currentGeofenceTopic.c_str(),
        payload,
        true
    );

    bool eventSent = true;
    if (isEvent)
    {
        const String eventsTopic = geofenceEventsTopic();
        eventSent = mqttClient.publish(eventsTopic.c_str(), payload, false);

        if (!eventSent)
        {
            strncpy(
                pendingGeofenceEventPayload,
                payload,
                sizeof(pendingGeofenceEventPayload) - 1
            );
            pendingGeofenceEventPayload[
                sizeof(pendingGeofenceEventPayload) - 1
            ] = '\0';
            hasPendingGeofenceEvent = true;
        }
    }

    const bool sent = statusSent && eventSent;
    Serial.println(
        sent
            ? "[MQTT] GEOFENCE ENVOYE"
            : "[MQTT] ECHEC ENVOI GEOFENCE"
    );
    return sent;
}
