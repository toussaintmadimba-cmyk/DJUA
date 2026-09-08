"""Regression tests for the ESP32 MQTT-to-API data contract."""

import json
import unittest
from types import SimpleNamespace

from app import main


class MqttContractTests(unittest.TestCase):
    def setUp(self) -> None:
        with main.state_lock:
            main.latest_telemetry_by_device.clear()
            main.latest_geofence_by_device.clear()
            main.history.clear()
            main.processed_message_ids.clear()
            main.processed_message_id_set.clear()
            main.message_sequence = 0

    def publish(self, topic: str, payload: dict[str, object]) -> None:
        message = SimpleNamespace(
            topic=topic,
            payload=json.dumps(payload).encode("utf-8"),
        )
        main.on_message(None, None, message)

    def test_telemetry_preserves_direct_and_calculated_sensor_values(self) -> None:
        payload = {
            "kit_id": "DJUA-KIN-000001",
            "timestamp": "2026-09-08T12:00:00+01:00",
            "latitude": -4.3251,
            "longitude": 15.3222,
            "interval_seconds": 1800,
            "battery": {"voltage_v": 12.4, "current_a": 0.5, "power_w": 6.2},
            "solar": {"power_w": 0.0},
            "ac_load": {"apparent_power_va": 0.0},
        }
        self.publish("djua/test/DJUA-KIN-000001/telemetry", payload)

        record = main.latest_telemetry_by_device["DJUA-KIN-000001"]
        self.assertEqual(record["message_type"], main.MESSAGE_TYPE_TELEMETRY)
        self.assertEqual(record["data"], payload)

    def test_geofence_snapshot_and_event_reach_the_api(self) -> None:
        payload = {
            "kit_id": "DJUA-KIN-000001",
            "state": "OUTSIDE",
            "event": "GEOFENCE_EXIT",
            "position_usable": True,
            "distance_m": 600.0,
            "confirmation": {"count": 3, "required": 3},
        }
        self.publish("djua/test/DJUA-KIN-000001/geofence", payload)
        self.publish("djua/test/DJUA-KIN-000001/geofence/events", payload)

        self.assertEqual(len(main.history), 2)
        self.assertEqual(
            main.history[0]["message_type"], main.MESSAGE_TYPE_GEOFENCE
        )
        self.assertEqual(
            main.history[1]["message_type"], main.MESSAGE_TYPE_GEOFENCE_EVENT
        )
        self.assertEqual(
            main.latest_geofence_by_device["DJUA-KIN-000001"]["data"], payload
        )

    def test_non_data_status_topic_is_not_treated_as_sensor_data(self) -> None:
        message = SimpleNamespace(
            topic="djua/test/DJUA-KIN-000001/status",
            payload=b"online",
        )
        main.on_message(None, None, message)
        self.assertFalse(main.history)

    def test_qos_retry_with_the_same_message_id_is_deduplicated(self) -> None:
        payload = {"kit_id": "DJUA-KIN-000001", "message_id": "DJUA-KIN-000001-7"}
        self.publish("djua/test/DJUA-KIN-000001/telemetry", payload)
        self.publish("djua/test/DJUA-KIN-000001/telemetry", payload)
        self.assertEqual(len(main.history), 1)


if __name__ == "__main__":
    unittest.main()
