import sys
import os
import pytest
from fastapi.testclient import TestClient
from backend.app.main import app

def test_full_vertical_end_to_end_flow():
    client = TestClient(app)

    print("\n--- STEP 1: Authenticate Admin User ---")
    login_res = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "Admin@SmartGrow2026!"
    })
    assert login_res.status_code == 200, f"Login failed: {login_res.text}"
    token = login_res.json()["access_token"]
    auth_headers = {"Authorization": f"Bearer {token}"}
    print("  [OK] Admin authenticated, JWT acquired.")

    print("\n--- STEP 2: Enroll ESP32-S3 Edge Controller ---")
    reg_res = client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "name": "Main Testbed ESP32-S3",
        "registration_token": "smartgrow-device-enrollment-secret-2026",
        "firmware_version": "1.0.0",
        "model_version": "v1.0.0-dt"
    })
    assert reg_res.status_code in [200, 201], f"Device registration failed: {reg_res.text}"
    print("  [OK] Device esp32s3-greenhouse-e2e-01 successfully registered.")

    print("\n--- STEP 3: Firmware Ingests Telemetry (Soil, Temp, Hum, Light, TinyML Class) ---")
    telemetry_payload = {
        "schema_version": "1.0.0",
        "device_id": "esp32s3-greenhouse-e2e-01",
        "boot_id": "boot-e2e-001",
        "sequence": 101,
        "soil_moisture_pct": 28.5,
        "temperature_c": 27.2,
        "humidity_pct": 62.0,
        "light_level_pct": 85.0,
        "ai_class": 1,  # NEED_WATER
        "ai_confidence": 0.98,
        "model_version": "v1.0.0-dt",
        "firmware_version": "1.0.0",
        "pump_state": False,
        "operating_mode": "AUTO",
        "health_status": "HEALTHY"
    }
    telem_res = client.post("/api/v1/telemetry", json=telemetry_payload)
    assert telem_res.status_code == 201, f"Telemetry ingest failed: {telem_res.text}"
    reading_id = telem_res.json()["id"]
    print(f"  [OK] Telemetry ingested, recorded reading ID #{reading_id}.")

    print("\n--- STEP 4: Frontend Fetches Latest Telemetry & Dashboard Summary ---")
    latest_res = client.get("/api/v1/telemetry/latest?device_id=esp32s3-greenhouse-e2e-01", headers=auth_headers)
    assert latest_res.status_code == 200
    latest_data = latest_res.json()
    assert latest_data["soil_moisture_pct"] == 28.5
    assert latest_data["ai_class"] == 1
    print(f"  [OK] Latest data verified: Soil={latest_data['soil_moisture_pct']}%, AI Class={latest_data['ai_class']}.")

    summary_res = client.get("/api/v1/analytics/dashboard-summary", headers=auth_headers)
    assert summary_res.status_code == 200
    summary = summary_res.json()
    assert summary["online_devices"] >= 1
    print(f"  [OK] Dashboard summary calculated: {summary['online_devices']} devices online.")

    print("\n--- STEP 5: Operator Dispatches Irrigation Command (30s) ---")
    cmd_res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "duration_seconds": 30
    }, headers=auth_headers)
    assert cmd_res.status_code == 200
    cmd_id = cmd_res.json()["command_id"]
    print(f"  [OK] Irrigation command created: #{cmd_id} (Status: pending).")

    print("\n--- STEP 6: ESP32-S3 Polls Pending Commands ---")
    poll_res = client.get("/api/v1/devices/esp32s3-greenhouse-e2e-01/poll-commands")
    assert poll_res.status_code == 200
    pending_cmds = poll_res.json()
    assert len(pending_cmds) >= 1
    target_cmd = next(c for c in pending_cmds if c["command_id"] == cmd_id)
    assert target_cmd["command_type"] == "PUMP_START"
    print(f"  [OK] ESP32-S3 polled and received command #{cmd_id}.")

    print("\n--- STEP 7: ESP32-S3 Firmware Executes and Returns ACK ---")
    ack_res = client.post("/api/v1/devices/esp32s3-greenhouse-e2e-01/commands/ack", json={
        "command_id": cmd_id,
        "status": "completed",
        "result_message": "Relay activated, pump running for 30s",
        "error_code": 0
    })
    assert ack_res.status_code == 200
    assert ack_res.json()["status"] == "completed"
    print("  [OK] ACK confirmed, command status updated to 'completed'.")

    print("\n--- STEP 8: Safety Guardrail Validation (>120s duration rejection) ---")
    safety_violation_res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "duration_seconds": 240 # Above 120s limit!
    }, headers=auth_headers)
    assert safety_violation_res.status_code == 422
    print("  [OK] Excessive runtime command safely rejected by guardrails.")

    print("\n=======================================================")
    print("  FULL VERTICAL END-TO-END FLOW SUCCESSFULLY VERIFIED  ")
    print("=======================================================\n")

if __name__ == "__main__":
    test_full_vertical_end_to_end_flow()
