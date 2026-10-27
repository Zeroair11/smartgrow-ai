import pytest
import time
from backend.app.core.security import get_password_hash
from backend.app.models.entities import User

def seed_viewer_user(db):
    viewer = db.query(User).filter(User.email == "viewer@smartgrow.local").first()
    if not viewer:
        viewer = User(
            email="viewer@smartgrow.local",
            hashed_password=get_password_hash("Viewer@SmartGrow2026!"),
            full_name="Viewer ReadOnly",
            role="viewer",
            is_active=True,
            is_superuser=False
        )
        db.add(viewer)
        db.commit()
    return viewer

def get_viewer_token(client, db):
    seed_viewer_user(db)
    res = client.post("/api/v1/auth/login", data={
        "username": "viewer@smartgrow.local",
        "password": "Viewer@SmartGrow2026!"
    })
    assert res.status_code == 200
    return res.json()["access_token"]

def get_admin_token(client):
    res = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "Admin@SmartGrow2026!"
    })
    assert res.status_code == 200
    return res.json()["access_token"]

def test_rbac_viewer_denied_actuator_and_config_mutations(client, db):
    admin_token = get_admin_token(client)
    viewer_token = get_viewer_token(client, db)

    # 1. Register device with admin
    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-rbac-unit",
        "name": "RBAC Test Unit",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    viewer_headers = {"Authorization": f"Bearer {viewer_token}"}

    # 2. Viewer can READ telemetry and device info
    res = client.get("/api/v1/devices/esp32s3-rbac-unit", headers=viewer_headers)
    assert res.status_code == 200

    # 3. Viewer is FORBIDDEN from dispatching commands (403)
    res = client.post("/api/v1/devices/esp32s3-rbac-unit/commands", json={
        "command_type": "VENT_SET",
        "param_val": 45
    }, headers=viewer_headers)
    assert res.status_code == 403, f"Viewer command dispatch should be 403, got {res.status_code}"

    # 4. Viewer is FORBIDDEN from starting manual irrigation (403)
    res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-rbac-unit",
        "duration_seconds": 30
    }, headers=viewer_headers)
    assert res.status_code == 403, f"Viewer manual irrigation start should be 403, got {res.status_code}"

    # 5. Viewer is FORBIDDEN from creating automation rules (403)
    res = client.post("/api/v1/automation/rules", json={
        "name": "Unauthorized Rule",
        "sensor_metric": "soil_moisture_pct",
        "operator": "<",
        "threshold_val": 25.0,
        "action_type": "PUMP_START"
    }, headers=viewer_headers)
    assert res.status_code == 403, f"Viewer rule creation should be 403, got {res.status_code}"

def test_command_idempotency_deduplication(client):
    admin_token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {admin_token}"}

    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-idemp-dev",
        "name": "Idempotency Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    cmd_payload = {
        "command_type": "VENT_SET",
        "param_val": 90,
        "ttl_s": 300,
        "idempotency_key": "idemp-unique-request-xyz-101"
    }

    # First dispatch
    res1 = client.post("/api/v1/devices/esp32s3-idemp-dev/commands", json=cmd_payload, headers=headers)
    assert res1.status_code == 201
    cmd1_data = res1.json()

    # Second dispatch with identical idempotency key
    res2 = client.post("/api/v1/devices/esp32s3-idemp-dev/commands", json=cmd_payload, headers=headers)
    assert res2.status_code in [200, 201]
    cmd2_data = res2.json()

    # MUST return the exact same command_id without spawning a second pending command
    assert cmd1_data["command_id"] == cmd2_data["command_id"]

def test_global_pump_duration_safety_guardrail(client):
    admin_token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {admin_token}"}

    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-guardrail-dev",
        "name": "Guardrail Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # Direct command dispatch for PUMP_START with 240 seconds duration (> 120s limit)
    res = client.post("/api/v1/devices/esp32s3-guardrail-dev/commands", json={
        "command_type": "PUMP_START",
        "param_val": 240,
        "duration_s": 240
    }, headers=headers)
    assert res.status_code == 400
    assert "exceeds maximum hardware safety limit" in res.json()["detail"]

def test_command_terminal_state_cannot_be_overwritten(client):
    admin_token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {admin_token}"}

    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-state-dev",
        "name": "State Machine Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # Create command
    res = client.post("/api/v1/devices/esp32s3-state-dev/commands", json={
        "command_type": "VENT_SET",
        "param_val": 30
    }, headers=headers)
    cmd_id = res.json()["command_id"]

    # Transition to completed
    res = client.post("/api/v1/devices/esp32s3-state-dev/commands/ack", json={
        "command_id": cmd_id,
        "status": "completed",
        "result_message": "Done"
    })
    assert res.status_code == 200
    assert res.json()["status"] == "completed"

    # Attempt to transition terminal 'completed' command back to 'executing' or 'failed'
    res = client.post("/api/v1/devices/esp32s3-state-dev/commands/ack", json={
        "command_id": cmd_id,
        "status": "executing",
        "result_message": "Trying invalid transition"
    })
    assert res.status_code == 400
    assert "Invalid command state transition" in res.json()["detail"]

def test_telemetry_extreme_anomaly_and_duplicate_dedup(client, db):
    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-anomaly-dev",
        "name": "Anomaly Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # Ingest extreme temperature reading (58.0°C)
    telem = {
        "schema_version": "1.0.0",
        "device_id": "esp32s3-anomaly-dev",
        "boot_id": "boot-anom-1",
        "sequence": 50,
        "soil_moisture_pct": 45.0,
        "temperature_c": 58.0, # CRITICAL EXTREME
        "humidity_pct": 30.0,
        "light_level_pct": 95.0,
        "ai_class": 4 # SENSOR_FAULT
    }
    res = client.post("/api/v1/telemetry", json=telem)
    assert res.status_code == 201
    reading_id = res.json()["id"]

    # Verify duplicate submission (same boot_id and sequence) is deduplicated
    res_dup = client.post("/api/v1/telemetry", json=telem)
    assert res_dup.status_code == 201
    assert res_dup.json()["id"] == reading_id # Same record returned idempotently

    # Verify critical temperature alert was created
    admin_token = get_admin_token(client)
    res_alerts = client.get("/api/v1/alerts?device_id=esp32s3-anomaly-dev", headers={"Authorization": f"Bearer {admin_token}"})
    assert res_alerts.status_code == 200
    alerts = res_alerts.json()
    assert len(alerts) >= 1
    assert any(a["severity"] == "critical" and "58.0" in a["title"] for a in alerts)
