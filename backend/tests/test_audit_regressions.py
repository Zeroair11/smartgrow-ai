import pytest
from datetime import datetime, timezone, timedelta
from backend.app.models.entities import Device, SensorReading, DeviceCommand
from backend.app.core.config import settings

def test_telemetry_csv_export_with_null_sensor_readings(client, db):
    # Setup test device
    dev = Device(
        device_id="ESP32_REG_TEST_01",
        name="Regression Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-test",
        status="online"
    )
    db.add(dev)
    db.commit()

    # Add a sensor reading with None values (simulating hardware sensor disconnect/failure)
    reading = SensorReading(
        device_id="ESP32_REG_TEST_01",
        sequence=1,
        soil_moisture_pct=None,
        temperature_c=None,
        humidity_pct=None,
        light_level_pct=None,
        ai_class="UNKNOWN",
        ai_confidence=None,
        pump_state=False,
        operating_mode="SAFE",
        recorded_at=datetime.now(timezone.utc)
    )
    db.add(reading)
    db.commit()

    # Login as admin to get auth token
    login_resp = client.post(
        "/api/v1/auth/login",
        data={"username": "admin@smartgrow.local", "password": "Admin@SmartGrow2026!"}
    )
    assert login_resp.status_code == 200
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    # Request CSV export
    response = client.get("/api/v1/telemetry/export-csv?device_id=ESP32_REG_TEST_01", headers=headers)
    assert response.status_code == 200
    assert "text/csv" in response.headers.get("content-type", "")
    content = response.text
    assert "ESP32_REG_TEST_01" in content
    assert "recorded_at,device_id,sequence" in content


def test_command_polling_includes_expires_at_sec_and_param_val(client, db):
    # Setup test device
    dev = Device(
        device_id="ESP32_CMD_TEST_01",
        name="Command Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-test",
        status="online"
    )
    db.add(dev)
    db.commit()

    # Login as admin to get auth token
    login_resp = client.post(
        "/api/v1/auth/login",
        data={"username": "admin@smartgrow.local", "password": "Admin@SmartGrow2026!"}
    )
    assert login_resp.status_code == 200
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    # Create a command with param_val and ttl_s
    cmd_resp = client.post(
        "/api/v1/devices/ESP32_CMD_TEST_01/commands",
        json={
            "command_type": "PUMP_START",
            "param_val": 45.0,
            "duration_s": 45,
            "ttl_s": 300,
            "idempotency_key": "idemp_test_ttl_001"
        },
        headers=headers
    )
    assert cmd_resp.status_code == 201
    created_cmd = cmd_resp.json()
    assert created_cmd["expires_at_sec"] > 0
    assert created_cmd["param_val"] == 45.0
    assert created_cmd["duration_s"] == 45

    # Firmware poll-commands endpoint
    poll_resp = client.get("/api/v1/devices/ESP32_CMD_TEST_01/poll-commands")
    assert poll_resp.status_code == 200
    commands = poll_resp.json()
    assert len(commands) == 1
    c = commands[0]
    assert c["command_type"] == "PUMP_START"
    assert "expires_at_sec" in c
    assert isinstance(c["expires_at_sec"], int)
    assert c["expires_at_sec"] > 0
    assert c["param_val"] == 45.0
    assert c["duration_s"] == 45


def test_assign_device_zone_optional_query_param(client, db):
    # Setup test device
    dev = Device(
        device_id="ESP32_ZONE_TEST_01",
        name="Zone Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-test",
        status="online",
        zone_id=1
    )
    db.add(dev)
    db.commit()

    # Login as admin
    login_resp = client.post(
        "/api/v1/auth/login",
        data={"username": "admin@smartgrow.local", "password": "Admin@SmartGrow2026!"}
    )
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    # Call PUT /devices/{device_id}/zone with NO zone_id parameter (unassigning zone)
    resp = client.put("/api/v1/devices/ESP32_ZONE_TEST_01/zone", headers=headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data["zone_id"] is None


def test_backend_sweeps_expired_commands_on_poll(client, db):
    # Setup test device
    dev = Device(
        device_id="ESP32_SWEEP_TEST_01",
        name="Sweep Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-test",
        status="online"
    )
    db.add(dev)
    db.commit()

    now = datetime.now(timezone.utc)
    # Add an expired command directly to DB
    expired_cmd = DeviceCommand(
        command_id="cmd-expired-db-001",
        device_id="ESP32_SWEEP_TEST_01",
        command_type="PUMP_START",
        status="pending",
        created_at=now - timedelta(seconds=600),
        expires_at=now - timedelta(seconds=100) # expired 100s ago
    )
    # Add a valid unexpired command
    valid_cmd = DeviceCommand(
        command_id="cmd-valid-db-002",
        device_id="ESP32_SWEEP_TEST_01",
        command_type="PUMP_START",
        status="pending",
        created_at=now,
        expires_at=now + timedelta(seconds=300)
    )
    db.add(expired_cmd)
    db.add(valid_cmd)
    db.commit()

    # Poll commands
    poll_resp = client.get("/api/v1/devices/ESP32_SWEEP_TEST_01/poll-commands")
    assert poll_resp.status_code == 200
    returned_cmds = poll_resp.json()

    # Expired command must NOT be returned
    assert len(returned_cmds) == 1
    assert returned_cmds[0]["command_id"] == "cmd-valid-db-002"

    # Expired command in DB must have transitioned to status='expired'
    db.refresh(expired_cmd)
    assert expired_cmd.status == "expired"
    assert expired_cmd.error_code == 101


def test_backend_rejects_invalid_ack_transitions(client, db):
    dev = Device(
        device_id="ESP32_TRANS_TEST_01",
        name="Transition Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-test",
        status="online"
    )
    db.add(dev)
    db.commit()

    now = datetime.now(timezone.utc)
    cmd = DeviceCommand(
        command_id="cmd-trans-001",
        device_id="ESP32_TRANS_TEST_01",
        command_type="PUMP_START",
        status="pending",
        created_at=now,
        expires_at=now + timedelta(seconds=300)
    )
    db.add(cmd)
    db.commit()

    # Transition from pending -> completed is allowed
    ack_resp = client.post(
        "/api/v1/devices/ESP32_TRANS_TEST_01/commands/ack",
        json={"command_id": "cmd-trans-001", "status": "completed", "result_message": "Success"}
    )
    assert ack_resp.status_code == 200
    assert ack_resp.json()["status"] == "completed"

    # Transition from completed -> executing is NOT allowed (completed is a terminal state)
    invalid_resp = client.post(
        "/api/v1/devices/ESP32_TRANS_TEST_01/commands/ack",
        json={"command_id": "cmd-trans-001", "status": "executing", "result_message": "Restart"}
    )
    assert invalid_resp.status_code == 400
    assert "Invalid command state transition" in invalid_resp.json()["detail"]


def test_backend_idempotency_key_deduplication(client, db):
    dev = Device(
        device_id="ESP32_IDEMP_TEST_01",
        name="Idemp Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-test",
        status="online"
    )
    db.add(dev)
    db.commit()

    # Login as admin
    login_resp = client.post(
        "/api/v1/auth/login",
        data={"username": "admin@smartgrow.local", "password": "Admin@SmartGrow2026!"}
    )
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    # 1. First command with idempotency key
    resp1 = client.post(
        "/api/v1/devices/ESP32_IDEMP_TEST_01/commands",
        json={
            "command_type": "VENT_SET",
            "param_val": 45.0,
            "idempotency_key": "idemp-unique-token-999"
        },
        headers=headers
    )
    assert resp1.status_code == 201
    cmd1 = resp1.json()

    # 2. Resend with exact same idempotency key
    resp2 = client.post(
        "/api/v1/devices/ESP32_IDEMP_TEST_01/commands",
        json={
            "command_type": "VENT_SET",
            "param_val": 90.0, # Attempting different value
            "idempotency_key": "idemp-unique-token-999"
        },
        headers=headers
    )
    assert resp2.status_code == 201
    cmd2 = resp2.json()

    # Deduplication check: Must return the EXACT SAME command_id
    assert cmd1["command_id"] == cmd2["command_id"]
    assert cmd1["id"] == cmd2["id"]

