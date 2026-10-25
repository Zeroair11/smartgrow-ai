import pytest

def get_admin_token(client):
    res = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "Admin@SmartGrow2026!"
    })
    assert res.status_code == 200
    return res.json()["access_token"]

def test_auth_login_and_me(client):
    token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {token}"}
    res = client.get("/api/v1/auth/me", headers=headers)
    assert res.status_code == 200
    data = res.json()
    assert data["email"] == "admin@smartgrow.local"
    assert data["role"] == "admin"

def test_device_registration_and_list(client):
    token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {token}"}

    # 1. Register device
    dev_payload = {
        "device_id": "esp32s3-test-unit",
        "name": "Greenhouse Zone 1 Controller",
        "registration_token": "smartgrow-device-enrollment-secret-2026",
        "firmware_version": "1.0.0",
        "model_version": "v1.0.0-dt"
    }
    res = client.post("/api/v1/devices/register", json=dev_payload)
    assert res.status_code == 201
    assert res.json()["device_id"] == "esp32s3-test-unit"

    # 2. List devices
    res = client.get("/api/v1/devices", headers=headers)
    assert res.status_code == 200
    devices = res.json()
    assert len(devices) >= 1
    assert any(d["device_id"] == "esp32s3-test-unit" for d in devices)

def test_telemetry_ingest_and_history(client):
    token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {token}"}

    # Ensure device registered
    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-telemetry-dev",
        "name": "Test Telemetry Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # Ingest valid telemetry
    telemetry_data = {
        "schema_version": "1.0.0",
        "device_id": "esp32s3-telemetry-dev",
        "boot_id": "boot-1",
        "sequence": 1,
        "soil_moisture_pct": 32.5,
        "temperature_c": 28.4,
        "humidity_pct": 65.0,
        "light_level_pct": 80.0,
        "ai_class": 1,
        "ai_confidence": 0.95,
        "model_version": "v1.0.0-dt",
        "firmware_version": "1.0.0",
        "pump_state": False,
        "operating_mode": "AUTO",
        "health_status": "HEALTHY"
    }
    res = client.post("/api/v1/telemetry", json=telemetry_data)
    assert res.status_code == 201
    assert res.json()["device_id"] == "esp32s3-telemetry-dev"

    # Test history retrieval
    res = client.get("/api/v1/telemetry/history?device_id=esp32s3-telemetry-dev", headers=headers)
    assert res.status_code == 200
    history = res.json()
    assert len(history) == 1
    assert history[0]["sequence"] == 1

def test_telemetry_schema_validation(client):
    # Soil moisture out of bounds (> 100%)
    invalid_data = {
        "schema_version": "1.0.0",
        "device_id": "esp32s3-telemetry-dev",
        "sequence": 2,
        "soil_moisture_pct": 150.0, # INVALID
        "temperature_c": 28.4,
        "humidity_pct": 65.0,
        "light_level_pct": 80.0
    }
    res = client.post("/api/v1/telemetry", json=invalid_data)
    assert res.status_code == 422 # Pydantic validation error

def test_command_lifecycle(client):
    token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {token}"}

    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-cmd-dev",
        "name": "Command Test Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # 1. Create command
    cmd_payload = {
        "command_type": "VENT_SET",
        "param_val": 60,
        "ttl_s": 300
    }
    res = client.post("/api/v1/devices/esp32s3-cmd-dev/commands", json=cmd_payload, headers=headers)
    assert res.status_code == 201
    cmd_data = res.json()
    cmd_id = cmd_data["command_id"]
    assert cmd_data["status"] == "pending"

    # 2. Firmware polls command
    res = client.get("/api/v1/devices/esp32s3-cmd-dev/poll-commands")
    assert res.status_code == 200
    pending_cmds = res.json()
    assert len(pending_cmds) == 1
    assert pending_cmds[0]["command_id"] == cmd_id

    # 3. Firmware ACKs command completion
    ack_payload = {
        "command_id": cmd_id,
        "status": "completed",
        "result_message": "Vent roof angle set to 60 deg",
        "error_code": 0
    }
    res = client.post("/api/v1/devices/esp32s3-cmd-dev/commands/ack", json=ack_payload)
    assert res.status_code == 200
    assert res.json()["status"] == "completed"

def test_irrigation_safety_limits(client):
    token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {token}"}

    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-irrig-dev",
        "name": "Irrigation Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # Attempt to start pump for 300 seconds (exceeds safety max 120s)
    res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-irrig-dev",
        "duration_seconds": 300
    }, headers=headers)
    assert res.status_code == 422 # Rejected by Pydantic validation Field(le=120)

    # Valid duration (45 seconds)
    res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-irrig-dev",
        "duration_seconds": 45
    }, headers=headers)
    assert res.status_code == 200
    assert res.json()["command_type"] == "PUMP_START"

def test_automation_rules_evaluation(client):
    token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {token}"}

    client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-auto-dev",
        "name": "Automation Device",
        "registration_token": "smartgrow-device-enrollment-secret-2026"
    })

    # Create rule: When soil_moisture_pct < 30.0 -> Start Pump for 30s
    rule_payload = {
        "name": "Low Soil Moisture Auto-Water",
        "sensor_metric": "soil_moisture_pct",
        "operator": "<",
        "threshold_val": 30.0,
        "action_type": "PUMP_START",
        "action_param_val": 30.0,
        "cooldown_s": 300
    }
    res = client.post("/api/v1/automation/rules", json=rule_payload, headers=headers)
    assert res.status_code == 201

    # Send telemetry crossing the threshold (soil_moisture = 22.0)
    client.post("/api/v1/telemetry", json={
        "schema_version": "1.0.0",
        "device_id": "esp32s3-auto-dev",
        "sequence": 1,
        "soil_moisture_pct": 22.0,
        "temperature_c": 27.0,
        "humidity_pct": 60.0,
        "light_level_pct": 75.0
    })

    # Check pending commands for device - automation should have created a PUMP_START command
    res = client.get("/api/v1/devices/esp32s3-auto-dev/poll-commands")
    assert res.status_code == 200
    cmds = res.json()
    assert len(cmds) >= 1
    assert any(c["command_type"] == "PUMP_START" for c in cmds)
