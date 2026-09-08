# Djua Box Telemetry API

This FastAPI service receives telemetry published by the ESP32 and exposes it
to a dashboard or any other HTTP client. It works across different networks:
both the ESP32 and this API connect to the same MQTT broker over the internet.

## Run locally

From this `api` folder:

```powershell
uv sync
uv run uvicorn app.main:app --reload
```

Open `http://127.0.0.1:8000/docs` for the interactive API documentation.

## Test topic

The default configuration subscribes to every structured data topic of each
device:

```text
djua/test/+/#
```

That includes the firmware topics:

```text
djua/test/DJUA-KIN-000001/telemetry
djua/test/DJUA-KIN-000001/geofence
djua/test/DJUA-KIN-000001/geofence/events
```

## Endpoints

| Endpoint | Purpose |
| --- | --- |
| `GET /health` | MQTT connection state and received-device count |
| `GET /devices` | Devices that have sent telemetry |
| `GET /devices/{device_id}/telemetry/latest` | Most recent device message |
| `GET /devices/{device_id}/telemetry?limit=100` | Recent messages for a device |
| `GET /devices/{device_id}/geofence/latest` | Latest calculated geofence state |
| `GET /devices/{device_id}/geofence?limit=100` | Geofence snapshots and events |
| `WS /ws/telemetry` | Direct sensor values and calculated telemetry |
| `WS /ws/geofence` | Calculated geofence states and enter/exit events |

The telemetry record preserves every field emitted by the ESP32: RTC timestamp,
GPS coordinates, INA219 voltage/current/power, telemetry interval, and the
currently defined solar and AC fields. Geofence records preserve the position,
distance to the zone, state, transition event, and confirmation calculation.

## Delivery reliability

The current Arduino-compatible firmware uses PubSubClient with MQTT QoS 0. A
telemetry record emitted while the broker connection is unavailable is not
retried. Geofence events are temporarily retained in RAM until reconnection,
but do not survive an ESP32 restart.

Messages are stored in memory only (up to 500 by default), so restarting the
API clears its history. Set `TELEMETRY_HISTORY_LIMIT` to change that limit.

## Configuration

Copy `.env.example` to `.env` and set environment variables in the shell or
deployment configuration. The current app intentionally has no dotenv loader.

For production, replace the public test broker with an authenticated TLS broker
and replace the open CORS policy with the URL of your dashboard.
