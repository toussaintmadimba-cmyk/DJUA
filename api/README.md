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

The default configuration subscribes to:

```text
djua/test/+/telemetry
```

That includes the firmware topic:

```text
djua/test/DJUA-KIN-000001/telemetry
```

## Endpoints

| Endpoint | Purpose |
| --- | --- |
| `GET /health` | MQTT connection state and received-device count |
| `GET /devices` | Devices that have sent telemetry |
| `GET /devices/{device_id}/telemetry/latest` | Most recent device message |
| `GET /devices/{device_id}/telemetry?limit=100` | Recent messages for a device |
| `WS /ws/telemetry` | New telemetry messages as they arrive |

Messages are stored in memory only (up to 500 by default), so restarting the
API clears its history. Set `TELEMETRY_HISTORY_LIMIT` to change that limit.

## Configuration

Copy `.env.example` to `.env` and set environment variables in the shell or
deployment configuration. The current app intentionally has no dotenv loader.

For production, replace the public test broker with an authenticated TLS broker
and replace the open CORS policy with the URL of your dashboard.
