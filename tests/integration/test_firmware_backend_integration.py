"""
Level 3 Integration Test Suite: Firmware Network Client <-> Backend Contract Verification
Tests the exact REST API contract used by ESP32-S3 firmware:
- Device enrollment (/api/v1/devices/register)
- Telemetry ingestion (/api/v1/telemetry)
- Downlink command polling (/api/v1/devices/{id}/poll-commands)
- Command execution ACK reporting (/api/v1/devices/{id}/commands/ack)
- Safety constraint enforcement
"""

import sys
import os
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))

from fastapi.testclient import TestClient
from backend.app.main import app

client = TestClient(app)

def test_firmware_backend_contract():
    run_id = int(time.time())
    test_device_id = f"esp32s3-integ-{run_id}"

    # 1. Device Enrollment
    reg_payload = {
        "device_id": test_device_id,
        "name": f"ESP32-S3 Integration Device {run_id}",
        "registration_token": "smartgrow-device-enrollment-secret-2026",
        "firmware_version": "1.0.0",
        "model_version": "v1.0.0-dt",
        "mac_address": "b8:1f:3f:bb:d0:c4"
    }

    reg_res = client.post("/api/v1/devices/register", json=reg_payload)
    assert reg_res.status_code in [200, 201], f"Registration failed: {reg_res.text}"
    device_data = reg_res.json()
    assert device_data["device_id"] == test_device_id
    assert device_data["status"] == "online"

    # Reject invalid token
    bad_reg = client.post("/api/v1/devices/register", json={
        **reg_payload,
        "device_id": f"bad-dev-{run_id}",
        "registration_token": "wrong-secret-token"
    })
    assert bad_reg.status_code == 401

    # 2. Telemetry Ingestion from Firmware
    telem_payload = {
        "schema_version": "1.0.0",
        "device_id": test_device_id,
        "boot_id": "boot-test-001",
        "sequence": 1,
        "timestamp_ms": 15000,
        "soil_moisture_pct": 36.5,
        "temperature_c": 26.8,
        "humidity_pct": 64.0,
        "light_level_pct": 78.5,
        "ai_class": 0,  # NORMAL
        "ai_confidence": 0.965,
        "model_version": "v1.0.0-dt",
        "firmware_version": "1.0.0",
        "pump_state": False,
        "operating_mode": "AUTO",
        "health_status": "HEALTHY"
    }

    telem_res = client.post("/api/v1/telemetry", json=telem_payload)
    assert telem_res.status_code == 201, f"Telemetry failed: {telem_res.text}"
    reading = telem_res.json()
    assert reading["device_id"] == test_device_id
    assert reading["soil_moisture_pct"] == 36.5
    assert reading["soil_valid"] is True
    assert reading["dht_valid"] is True

    # 2b. Faulted Telemetry Ingestion (Sensor Fault -> Null values, validity=False)
    fault_payload = {
        "schema_version": "1.0.0",
        "device_id": test_device_id,
        "boot_id": "boot-test-001",
        "sequence": 2,
        "timestamp_ms": 17000,
        "soil_moisture_pct": None,
        "temperature_c": None,
        "humidity_pct": None,
        "light_level_pct": 78.5,
        "soil_valid": False,
        "dht_valid": False,
        "light_valid": True,
        "ai_class": 4,  # SENSOR_FAULT
        "ai_confidence": 1.0,
        "model_version": "v1.0.0-dt",
        "firmware_version": "1.0.0",
        "pump_state": False,
        "operating_mode": "AUTO",
        "health_status": "SENSOR_FAULT"
    }

    fault_res = client.post("/api/v1/telemetry", json=fault_payload)
    assert fault_res.status_code == 201, f"Faulted telemetry failed: {fault_res.text}"
    fault_reading = fault_res.json()
    assert fault_reading["soil_moisture_pct"] is None
    assert fault_reading["temperature_c"] is None
    assert fault_reading["soil_valid"] is False
    assert fault_reading["dht_valid"] is False

    # 3. Downlink Command Creation (Operator -> Backend)
    admin_login = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "Admin@SmartGrow2026!"
    })
    assert admin_login.status_code == 200
    token = admin_login.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    cmd_payload = {
        "command_type": "PUMP_START",
        "param_val": 45,
        "duration_s": 45,
        "ttl_s": 120,
        "idempotency_key": f"integ-cmd-{run_id}"
    }

    cmd_res = client.post(f"/api/v1/devices/{test_device_id}/commands", json=cmd_payload, headers=headers)
    assert cmd_res.status_code == 201
    cmd_id = cmd_res.json()["command_id"]
    assert cmd_res.json()["status"] == "pending"

    # 4. Downlink Command Polling (Firmware -> Backend)
    poll_res = client.get(f"/api/v1/devices/{test_device_id}/poll-commands")
    assert poll_res.status_code == 200
    polled = poll_res.json()
    assert len(polled) >= 1
    target_cmd = next((c for c in polled if c["command_id"] == cmd_id), None)
    assert target_cmd is not None
    assert target_cmd["command_type"] == "PUMP_START"

    # 5. Command Execution ACK Dispatch (Firmware -> Backend)
    ack_payload = {
        "command_id": cmd_id,
        "status": "completed",
        "result_message": "Pump started for 45 sec",
        "error_code": 0
    }
    ack_res = client.post(f"/api/v1/devices/{test_device_id}/commands/ack", json=ack_payload)
    assert ack_res.status_code == 200
    updated_cmd = ack_res.json()
    assert updated_cmd["status"] == "completed"
    assert updated_cmd["error_code"] == 0

    # 6. Verify Terminal State Transition Constraint
    # Once completed, cannot transition to executing or acknowledged
    invalid_ack = client.post(f"/api/v1/devices/{test_device_id}/commands/ack", json={
        "command_id": cmd_id,
        "status": "executing",
        "result_message": "Re-executing"
    })
    assert invalid_ack.status_code == 400

    print("--- [PASS] Level 3 Firmware-Backend Contract Verification Completed Successfully ---")

if __name__ == "__main__":
    test_firmware_backend_contract()
