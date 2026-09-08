"""Djua Box telemetry API.

The service subscribes to MQTT in the background and exposes the received
telemetry through HTTP endpoints and a polling WebSocket stream.
"""

import asyncio
import json
import logging
import os
import threading
from collections import deque
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Any

import paho.mqtt.client as mqtt
from fastapi import FastAPI, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("djua-api")

MQTT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "test.mosquitto.org")
MQTT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "1883"))
# Subscribe to every structured data channel emitted by each device:
# telemetry, geofence snapshots, and geofence events.
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "djua/test/+/#")
HISTORY_LIMIT = int(os.getenv("TELEMETRY_HISTORY_LIMIT", "500"))

state_lock = threading.Lock()
latest_telemetry_by_device: dict[str, dict[str, Any]] = {}
latest_geofence_by_device: dict[str, dict[str, Any]] = {}
history: deque[dict[str, Any]] = deque(maxlen=HISTORY_LIMIT)
mqtt_state = {"connected": False, "last_error": None}
message_sequence = 0

MESSAGE_TYPE_TELEMETRY = "telemetry"
MESSAGE_TYPE_GEOFENCE = "geofence"
MESSAGE_TYPE_GEOFENCE_EVENT = "geofence_event"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def device_id_for(topic: str, payload: dict[str, Any]) -> str:
    """Use kit_id when supplied; fall back to the MQTT topic structure."""
    kit_id = payload.get("kit_id")
    if isinstance(kit_id, str) and kit_id:
        return kit_id

    parts = topic.split("/")
    return parts[-2] if len(parts) >= 2 else "unknown"


def message_type_for(topic: str) -> str | None:
    """Return the known device-data type represented by an MQTT topic."""
    parts = topic.split("/")
    if len(parts) == 4 and parts[-1] == "telemetry":
        return MESSAGE_TYPE_TELEMETRY
    if len(parts) == 4 and parts[-1] == "geofence":
        return MESSAGE_TYPE_GEOFENCE
    if len(parts) == 5 and parts[-2:] == ["geofence", "events"]:
        return MESSAGE_TYPE_GEOFENCE_EVENT
    return None


def on_connect(
    client: mqtt.Client,
    _userdata: Any,
    _flags: Any,
    reason_code: Any,
    _properties: Any,
) -> None:
    if reason_code != 0:
        message = f"MQTT connection refused: {reason_code}"
        logger.error(message)
        with state_lock:
            mqtt_state.update(connected=False, last_error=message)
        return

    client.subscribe(MQTT_TOPIC, qos=0)
    logger.info("Subscribed to MQTT topic %s", MQTT_TOPIC)
    with state_lock:
        mqtt_state.update(connected=True, last_error=None)


def on_disconnect(
    _client: mqtt.Client,
    _userdata: Any,
    _disconnect_flags: Any,
    reason_code: Any,
    _properties: Any,
) -> None:
    message = None if reason_code == 0 else f"MQTT disconnected: {reason_code}"
    if message:
        logger.warning(message)
    with state_lock:
        mqtt_state.update(connected=False, last_error=message)


def on_message(_client: mqtt.Client, _userdata: Any, message: mqtt.MQTTMessage) -> None:
    global message_sequence

    message_type = message_type_for(message.topic)
    if message_type is None:
        # The wildcard subscription can also receive non-data channels such as
        # the retained online/offline status. They are intentionally excluded
        # from the sensor-data API.
        logger.debug("Ignored MQTT topic outside the data contract: %s", message.topic)
        return

    try:
        payload = json.loads(message.payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        logger.warning("Ignored non-JSON payload on %s", message.topic)
        return

    if not isinstance(payload, dict):
        logger.warning("Ignored JSON payload that was not an object on %s", message.topic)
        return

    device_id = device_id_for(message.topic, payload)
    record = {
        "device_id": device_id,
        "topic": message.topic,
        "message_type": message_type,
        "received_at": utc_now(),
        "data": payload,
    }

    with state_lock:
        message_sequence += 1
        record["sequence"] = message_sequence
        if message_type == MESSAGE_TYPE_TELEMETRY:
            latest_telemetry_by_device[device_id] = record
        else:
            latest_geofence_by_device[device_id] = record
        history.append(record)

    logger.info("%s received from %s", message_type, device_id)


def make_mqtt_client() -> mqtt.Client:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="djua-telemetry-api")
    username = os.getenv("MQTT_USERNAME")
    password = os.getenv("MQTT_PASSWORD")
    if username:
        client.username_pw_set(username, password)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    return client


mqtt_client = make_mqtt_client()


@asynccontextmanager
async def lifespan(_app: FastAPI):
    logger.info("Starting MQTT client for %s:%s", MQTT_BROKER_HOST, MQTT_BROKER_PORT)
    mqtt_client.connect_async(MQTT_BROKER_HOST, MQTT_BROKER_PORT, keepalive=60)
    mqtt_client.loop_start()
    yield
    mqtt_client.loop_stop()
    mqtt_client.disconnect()


app = FastAPI(
    title="Djua Box Telemetry API",
    version="0.1.0",
    description="Receives Djua Box MQTT telemetry and makes it available over HTTP.",
    lifespan=lifespan,
)

# Convenient for a separately hosted test dashboard. Restrict these origins in production.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET"],
    allow_headers=["*"],
)


@app.get("/health")
def health() -> dict[str, Any]:
    with state_lock:
        return {
            "status": "ok",
            "mqtt": dict(mqtt_state),
            "subscribed_topic": MQTT_TOPIC,
            "known_devices": len(
                set(latest_telemetry_by_device) | set(latest_geofence_by_device)
            ),
        }


@app.get("/devices")
def devices() -> list[dict[str, Any]]:
    with state_lock:
        device_ids = set(latest_telemetry_by_device) | set(latest_geofence_by_device)
        return [
            {
                "device_id": device_id,
                "last_telemetry_at": latest_telemetry_by_device.get(device_id, {}).get(
                    "received_at"
                ),
                "last_geofence_at": latest_geofence_by_device.get(device_id, {}).get(
                    "received_at"
                ),
            }
            for device_id in sorted(device_ids)
        ]


@app.get("/devices/{device_id}/telemetry/latest")
def latest_telemetry(device_id: str) -> dict[str, Any]:
    with state_lock:
        record = latest_telemetry_by_device.get(device_id)
    if record is None:
        raise HTTPException(status_code=404, detail="No telemetry has been received for this device.")
    return record


@app.get("/devices/{device_id}/telemetry")
def telemetry_history(
    device_id: str,
    limit: int = Query(default=100, ge=1, le=HISTORY_LIMIT),
) -> list[dict[str, Any]]:
    with state_lock:
        matching = [
            record
            for record in history
            if record["device_id"] == device_id
            and record["message_type"] == MESSAGE_TYPE_TELEMETRY
        ]
    return matching[-limit:]


@app.get("/devices/{device_id}/geofence/latest")
def latest_geofence(device_id: str) -> dict[str, Any]:
    with state_lock:
        record = latest_geofence_by_device.get(device_id)
    if record is None:
        raise HTTPException(status_code=404, detail="No geofence data has been received for this device.")
    return record


@app.get("/devices/{device_id}/geofence")
def geofence_history(
    device_id: str,
    limit: int = Query(default=100, ge=1, le=HISTORY_LIMIT),
) -> list[dict[str, Any]]:
    with state_lock:
        matching = [
            record
            for record in history
            if record["device_id"] == device_id
            and record["message_type"]
            in {MESSAGE_TYPE_GEOFENCE, MESSAGE_TYPE_GEOFENCE_EVENT}
        ]
    return matching[-limit:]


async def stream_message_types(websocket: WebSocket, message_types: set[str]) -> None:
    """Stream only the requested classes of structured MQTT data."""
    await websocket.accept()
    last_sent_sequence = 0
    try:
        while True:
            with state_lock:
                records = [
                    record
                    for record in history
                    if record["sequence"] > last_sent_sequence
                    and record["message_type"] in message_types
                ]
            for record in records:
                await websocket.send_json(record)
                last_sent_sequence = record["sequence"]
            await asyncio.sleep(0.5)
    except WebSocketDisconnect:
        return


@app.websocket("/ws/telemetry")
async def telemetry_stream(websocket: WebSocket) -> None:
    """Stream sensor telemetry, including direct and calculated values."""
    await stream_message_types(websocket, {MESSAGE_TYPE_TELEMETRY})


@app.websocket("/ws/geofence")
async def geofence_stream(websocket: WebSocket) -> None:
    """Stream geofence snapshots and confirmed enter/exit events."""
    await stream_message_types(
        websocket,
        {MESSAGE_TYPE_GEOFENCE, MESSAGE_TYPE_GEOFENCE_EVENT},
    )
