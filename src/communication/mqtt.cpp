#include "mqtt.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_idf_version.h>
#include <esp_mqtt_client.h>

#include "../config.h"

namespace
{
constexpr uint32_t QUEUE_FILE_MAGIC = 0x444A5541UL; // "DJUA"
constexpr char QUEUE_TEMP_FILE[] = "/djua_mqtt.tmp";
constexpr char QUEUE_FILE_PREFIX[] = "/djua_mqtt_";

struct QueueFileHeader
{
    uint32_t magic;
    uint32_t sequence;
    uint16_t topicLength;
    uint16_t payloadLength;
    uint8_t retain;
};

Preferences preferences;
esp_mqtt_client_handle_t mqttClient = nullptr;
String brokerUriStorage;
String clientIdStorage;
String willTopicStorage;
bool queueAvailable = false;
volatile bool mqttConnected = false;
volatile int inFlightMessageId = -1;
uint32_t queueHead = 0;
uint16_t queueCount = 0;

String telemetryTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/telemetry";
}

String statusTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/status";
}

String geofenceTopic()
{
    return String(MQTT_TOPIC_PREFIX) + "/" + DEVICE_ID + "/geofence";
}

String geofenceEventsTopic()
{
    return geofenceTopic() + "/events";
}

String queueFilePath(uint32_t position)
{
    return String(QUEUE_FILE_PREFIX) +
           String(position % MQTT_QUEUE_MAX_MESSAGES) +
           ".bin";
}

void persistQueueMetadata()
{
    preferences.putULong("head", queueHead);
    preferences.putUShort("count", queueCount);
}

void discardHeadMessage()
{
    if (queueCount == 0)
    {
        return;
    }

    const String path = queueFilePath(queueHead);
    queueHead++;
    queueCount--;
    // Persist first: a reset here may cause a duplicate, but cannot silently
    // lose a reading that has not been acknowledged by the broker.
    persistQueueMetadata();
    LittleFS.remove(path);
}

bool initializePersistentQueue()
{
    if (!preferences.begin("djua-mqtt", false))
    {
        Serial.println("[MQTT] Preferences indisponible : file fiable desactivee.");
        return false;
    }

    const bool filesystemWasInitialized = preferences.getBool("fsready", false);
    if (!LittleFS.begin(false))
    {
        // A brand-new partition must be formatted once. After that first mount,
        // never auto-format: an error must not erase unsent measurements.
        if (filesystemWasInitialized || !LittleFS.begin(true))
        {
            Serial.println("[MQTT] LittleFS indisponible : file fiable desactivee.");
            return false;
        }
        preferences.putBool("fsready", true);
        Serial.println("[MQTT] LittleFS initialise pour la file persistante.");
    }
    else if (!filesystemWasInitialized)
    {
        preferences.putBool("fsready", true);
    }

    queueHead = preferences.getULong("head", 0);
    queueCount = preferences.getUShort("count", 0);

    if (queueCount > MQTT_QUEUE_MAX_MESSAGES)
    {
        Serial.println("[MQTT] Metadonnees de file invalides : remise a zero.");
        queueHead = 0;
        queueCount = 0;
        persistQueueMetadata();
    }

    return true;
}

uint32_t nextMessageSequence()
{
    // Zero is reserved as the Preferences default and never emitted.
    const uint32_t sequence = preferences.getULong("next", 1);
    const uint32_t following = sequence == UINT32_MAX ? 1 : sequence + 1;
    preferences.putULong("next", following);
    return sequence;
}

bool enqueueMessage(
    const char* topic,
    const char* payload,
    size_t payloadLength,
    bool retain,
    uint32_t sequence
)
{
    if (!queueAvailable)
    {
        Serial.println("[MQTT] Message refuse : file persistante indisponible.");
        return false;
    }

    const size_t topicLength = strlen(topic);
    if (topicLength == 0 || topicLength > MQTT_QUEUE_MAX_TOPIC_LENGTH ||
        payloadLength == 0 || payloadLength > MQTT_QUEUE_MAX_PAYLOAD_LENGTH)
    {
        Serial.println("[MQTT] Message refuse : taille MQTT invalide.");
        return false;
    }

    if (queueCount >= MQTT_QUEUE_MAX_MESSAGES)
    {
        Serial.println("[MQTT] File persistante pleine : message non perdu silencieusement.");
        return false;
    }

    const uint32_t tail = queueHead + queueCount;
    const String finalPath = queueFilePath(tail);
    File file = LittleFS.open(QUEUE_TEMP_FILE, FILE_WRITE);
    if (!file)
    {
        Serial.println("[MQTT] Impossible d'ecrire la file persistante.");
        return false;
    }

    const QueueFileHeader header = {
        QUEUE_FILE_MAGIC,
        sequence,
        static_cast<uint16_t>(topicLength),
        static_cast<uint16_t>(payloadLength),
        static_cast<uint8_t>(retain),
    };

    const bool written =
        file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) ==
            sizeof(header) &&
        file.write(reinterpret_cast<const uint8_t*>(topic), topicLength) == topicLength &&
        file.write(reinterpret_cast<const uint8_t*>(payload), payloadLength) == payloadLength;
    file.close();

    if (!written)
    {
        LittleFS.remove(QUEUE_TEMP_FILE);
        Serial.println("[MQTT] Ecriture incomplete dans la file persistante.");
        return false;
    }

    LittleFS.remove(finalPath);
    if (!LittleFS.rename(QUEUE_TEMP_FILE, finalPath))
    {
        LittleFS.remove(QUEUE_TEMP_FILE);
        Serial.println("[MQTT] Impossible de valider le message en file.");
        return false;
    }

    queueCount++;
    persistQueueMetadata();
    return true;
}

bool readHeadMessage(
    char* topic,
    size_t topicSize,
    char* payload,
    size_t payloadSize,
    bool* retain
)
{
    if (queueCount == 0)
    {
        return false;
    }

    const String path = queueFilePath(queueHead);
    File file = LittleFS.open(path, FILE_READ);
    QueueFileHeader header = {};
    const bool headerValid =
        file &&
        file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
        header.magic == QUEUE_FILE_MAGIC &&
        header.topicLength > 0 &&
        header.topicLength < topicSize &&
        header.payloadLength > 0 &&
        header.payloadLength < payloadSize;

    if (!headerValid)
    {
        if (file)
        {
            file.close();
        }
        Serial.println("[MQTT] Entree de file invalide : abandon de l'entree corrompue.");
        discardHeadMessage();
        return false;
    }

    const bool bodyValid =
        file.read(reinterpret_cast<uint8_t*>(topic), header.topicLength) == header.topicLength &&
        file.read(reinterpret_cast<uint8_t*>(payload), header.payloadLength) == header.payloadLength;
    file.close();

    if (!bodyValid)
    {
        Serial.println("[MQTT] Entree de file tronquee : abandon de l'entree corrompue.");
        discardHeadMessage();
        return false;
    }

    topic[header.topicLength] = '\0';
    payload[header.payloadLength] = '\0';
    *retain = header.retain != 0;
    return true;
}

void publishOnlineStatus()
{
    const String topic = statusTopic();
    esp_mqtt_client_publish(
        mqttClient,
        topic.c_str(),
        "online",
        0,
        MQTT_PUBLISH_QOS,
        true
    );
}

void pumpPersistentQueue()
{
    if (!mqttConnected || inFlightMessageId >= 0 || queueCount == 0)
    {
        return;
    }

    char topic[MQTT_QUEUE_MAX_TOPIC_LENGTH + 1] = {};
    char payload[MQTT_QUEUE_MAX_PAYLOAD_LENGTH + 1] = {};
    bool retain = false;
    if (!readHeadMessage(topic, sizeof(topic), payload, sizeof(payload), &retain))
    {
        return;
    }

    const int messageId = esp_mqtt_client_enqueue(
        mqttClient,
        topic,
        payload,
        0,
        MQTT_PUBLISH_QOS,
        retain,
        true
    );

    if (messageId < 0)
    {
        Serial.println("[MQTT] Outbox ESP-MQTT pleine ou publication refusee.");
        return;
    }

    // ESP-MQTT sends queued messages asynchronously. MQTT_EVENT_PUBLISHED
    // removes this persistent entry only after the broker PUBACK.
    inFlightMessageId = messageId;
}

bool queueJsonDocument(
    JsonDocument& document,
    const String& topic,
    bool retain
)
{
    if (!queueAvailable)
    {
        return false;
    }

    const uint32_t sequence = nextMessageSequence();
    char messageId[48] = {};
    snprintf(messageId, sizeof(messageId), "%s-%lu", DEVICE_ID, sequence);
    document["message_id"] = messageId;

    char payload[MQTT_QUEUE_MAX_PAYLOAD_LENGTH + 1] = {};
    const size_t payloadLength = serializeJson(document, payload, sizeof(payload));
    if (payloadLength == 0 || payloadLength >= sizeof(payload))
    {
        Serial.println("[MQTT] Erreur de construction JSON.");
        return false;
    }

    if (!enqueueMessage(topic.c_str(), payload, payloadLength, retain, sequence))
    {
        return false;
    }

    pumpPersistentQueue();
    return true;
}

#if ESP_IDF_VERSION_MAJOR >= 5
void mqttEventHandler(
    void*,
    esp_event_base_t,
    int32_t eventId,
    void* eventData
)
{
    const auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);
    if (eventId == MQTT_EVENT_CONNECTED)
    {
        mqttConnected = true;
        Serial.println("[MQTT] CONNECTE (QoS 1)");
        publishOnlineStatus();
    }
    else if (eventId == MQTT_EVENT_DISCONNECTED)
    {
        mqttConnected = false;
    }
    else if (eventId == MQTT_EVENT_PUBLISHED && event != nullptr &&
             event->msg_id == inFlightMessageId)
    {
        discardHeadMessage();
        inFlightMessageId = -1;
    }
}
#else
esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            mqttConnected = true;
            Serial.println("[MQTT] CONNECTE (QoS 1)");
            publishOnlineStatus();
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqttConnected = false;
            break;

        case MQTT_EVENT_PUBLISHED:
            if (event->msg_id == inFlightMessageId)
            {
                discardHeadMessage();
                inFlightMessageId = -1;
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}
#endif
} // namespace

void initMQTT()
{
    queueAvailable = initializePersistentQueue();
    if (!queueAvailable)
    {
        return;
    }

    brokerUriStorage = String("mqtt://") + MQTT_BROKER_HOST + ":" +
                       String(MQTT_BROKER_PORT);
    clientIdStorage = String("djua-") + DEVICE_ID;
    willTopicStorage = statusTopic();

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_mqtt_client_config_t config = {};
    config.broker.address.uri = brokerUriStorage.c_str();
    config.credentials.client_id = clientIdStorage.c_str();
    config.session.disable_clean_session = true;
    config.session.keepalive = 60;
    config.session.last_will.topic = willTopicStorage.c_str();
    config.session.last_will.msg = "offline";
    config.session.last_will.qos = MQTT_PUBLISH_QOS;
    config.session.last_will.retain = true;
    config.buffer.size = MQTT_QUEUE_MAX_PAYLOAD_LENGTH;
#else
    esp_mqtt_client_config_t config = {};
    config.uri = brokerUriStorage.c_str();
    config.client_id = clientIdStorage.c_str();
    config.disable_clean_session = true;
    config.keepalive = 60;
    config.lwt_topic = willTopicStorage.c_str();
    config.lwt_msg = "offline";
    config.lwt_qos = MQTT_PUBLISH_QOS;
    config.lwt_retain = true;
    config.buffer_size = MQTT_QUEUE_MAX_PAYLOAD_LENGTH;
    config.event_handle = mqttEventHandler;
#endif

    mqttClient = esp_mqtt_client_init(&config);
    if (mqttClient == nullptr)
    {
        Serial.println("[MQTT] Creation ESP-MQTT impossible.");
        return;
    }

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_mqtt_client_register_event(
        mqttClient,
        ESP_EVENT_ANY_ID,
        mqttEventHandler,
        nullptr
    );
#endif

    if (esp_mqtt_client_start(mqttClient) != ESP_OK)
    {
        Serial.println("[MQTT] Demarrage ESP-MQTT impossible.");
        mqttClient = nullptr;
        return;
    }

    Serial.print("[MQTT] File persistante initialisee : ");
    Serial.print(queueCount);
    Serial.println(" message(s) en attente.");
}

void updateMQTT()
{
    pumpPersistentQueue();
}

bool isMQTTConnected()
{
    return mqttConnected;
}

uint16_t pendingMQTTMessageCount()
{
    return queueCount;
}

bool sendTelemetryToMQTT(const TelemetryData& data)
{
    StaticJsonDocument<1024> doc;
    doc["kit_id"] = DEVICE_ID;
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

    const bool queued = queueJsonDocument(doc, telemetryTopic(), false);
    Serial.println(queued ? "[MQTT] TELEMETRIE MISE EN FILE" : "[MQTT] ECHEC FILE TELEMETRIE");
    return queued;
}

bool sendGeofenceToMQTT(
    const GeofenceResult& result,
    const char* timestamp,
    bool timestampValid
)
{
    StaticJsonDocument<768> doc;
    doc["kit_id"] = result.deviceId;
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
    confirmation["candidate_state"] = geofenceStateToString(result.candidateState);
    confirmation["count"] = result.confirmationCount;
    confirmation["required"] = GEOFENCE_CONFIRM_COUNT;

    const bool statusQueued = queueJsonDocument(doc, geofenceTopic(), true);
    if (!statusQueued)
    {
        return false;
    }

    if (result.eventType == GEOFENCE_EVENT_NONE)
    {
        return true;
    }

    // An event receives its own message_id and remains independently durable.
    return queueJsonDocument(doc, geofenceEventsTopic(), false);
}
