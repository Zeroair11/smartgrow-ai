import pytest
import json

def get_admin_token(client):
    res = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "Admin@SmartGrow2026!"
    })
    assert res.status_code == 200
    return res.json()["access_token"]

def test_user_crud_and_safety_guardrails(client):
    admin_token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {admin_token}"}

    # 1. Create a test operator user
    user_payload = {
        "email": "operator_test@smartgrow.io",
        "password": "StrongPassword123!",
        "full_name": "Test Operator",
        "role": "operator"
    }
    create_res = client.post("/api/v1/auth/users", json=user_payload, headers=headers)
    assert create_res.status_code == 201
    user_data = create_res.json()
    user_id = user_data["id"]
    assert user_data["email"] == "operator_test@smartgrow.io"
    assert user_data["role"] == "operator"
    assert user_data["is_active"] is True

    # 2. Get user by ID
    get_res = client.get(f"/api/v1/auth/users/{user_id}", headers=headers)
    assert get_res.status_code == 200
    assert get_res.json()["email"] == "operator_test@smartgrow.io"

    # 3. List users with search filter
    list_res = client.get("/api/v1/auth/users?q=operator_test", headers=headers)
    assert list_res.status_code == 200
    found_users = list_res.json()
    assert len(found_users) == 1
    assert found_users[0]["id"] == user_id

    # 4. Update user info
    patch_res = client.patch(f"/api/v1/auth/users/{user_id}", json={
        "full_name": "Senior Operator",
        "role": "operator"
    }, headers=headers)
    assert patch_res.status_code == 200
    assert patch_res.json()["full_name"] == "Senior Operator"

    # 5. Guardrail: Attempting to demote or deactivate the only admin should fail
    admin_info_res = client.get("/api/v1/auth/me", headers=headers)
    admin_id = admin_info_res.json()["id"]

    # Demote admin to operator -> MUST FAIL with 400
    demote_res = client.patch(f"/api/v1/auth/users/{admin_id}", json={
        "role": "operator"
    }, headers=headers)
    assert demote_res.status_code == 400
    assert "last active administrator" in demote_res.json()["detail"].lower()

    # Deactivate admin -> MUST FAIL with 400
    deact_res = client.patch(f"/api/v1/auth/users/{admin_id}", json={
        "is_active": False
    }, headers=headers)
    assert deact_res.status_code == 400
    assert "last active administrator" in deact_res.json()["detail"].lower()

    # Delete admin self -> MUST FAIL with 400
    del_admin_res = client.delete(f"/api/v1/auth/users/{admin_id}", headers=headers)
    assert del_admin_res.status_code == 400

    # 6. Deactivate and delete test operator user
    del_op_res = client.delete(f"/api/v1/auth/users/{user_id}", headers=headers)
    assert del_op_res.status_code == 200
    assert "deactivated" in del_op_res.json()["message"]

    # Verify user is deactivated
    check_res = client.get(f"/api/v1/auth/users/{user_id}", headers=headers)
    assert check_res.status_code == 200
    assert check_res.json()["is_active"] is False

def test_device_crud_and_revocation(client):
    admin_token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {admin_token}"}

    device_id = "esp32s3-crud-test-node"

    # 1. Register device
    reg_res = client.post("/api/v1/devices/register", json={
        "device_id": device_id,
        "name": "Fleet Node Alpha",
        "registration_token": "smartgrow-device-enrollment-secret-2026",
        "firmware_version": "1.0.0",
        "model_version": "v1.0"
    })
    assert reg_res.status_code in [200, 201]

    # 2. Patch device (update name and status)
    patch_res = client.patch(f"/api/v1/devices/{device_id}", json={
        "name": "Fleet Node Alpha Renamed"
    }, headers=headers)
    assert patch_res.status_code == 200
    assert patch_res.json()["name"] == "Fleet Node Alpha Renamed"

    # 3. Revoke device credentials
    revoke_res = client.post(f"/api/v1/devices/{device_id}/revoke", headers=headers)
    assert revoke_res.status_code == 200
    assert revoke_res.json()["status"] == "revoked"

    # 4. Verify telemetry from revoked device is rejected with 403 Forbidden
    telem_res = client.post("/api/v1/telemetry", json={
        "schema_version": "1.0.0",
        "device_id": device_id,
        "boot_id": "boot-test",
        "sequence": 1,
        "soil_moisture_pct": 28.0,
        "temperature_c": 25.0,
        "humidity_pct": 60.0,
        "light_level_pct": 80.0,
        "soil_valid": True,
        "dht_valid": True,
        "light_valid": True,
        "ai_class": 0,
        "ai_confidence": 0.95,
        "model_version": "v1.0",
        "firmware_version": "1.0.0",
        "pump_state": 0,
        "operating_mode": "AUTO",
        "health_status": "NORMAL"
    })
    assert telem_res.status_code == 403

    # 5. Verify polling commands from revoked device is rejected with 403 Forbidden
    poll_res = client.get(f"/api/v1/devices/{device_id}/poll-commands")
    assert poll_res.status_code == 403

    # 6. Verify sending commands to revoked device is rejected with 403 Forbidden
    cmd_res = client.post(f"/api/v1/devices/{device_id}/commands", json={
        "command_type": "PUMP_START",
        "param_val": 30
    }, headers=headers)
    assert cmd_res.status_code == 403

    # 7. Verify deploying OTA to revoked device is rejected with 403 Forbidden
    rel_res = client.post("/api/v1/ota/releases", data={"version": "9.9.9", "release_notes": "Test"}, files={"firmware_bin": ("fw.bin", b"fake binary data")}, headers=headers)
    assert rel_res.status_code in [200, 201]
    rel_id = rel_res.json()["id"]

    ota_res = client.post(f"/api/v1/ota/deploy/{rel_id}/{device_id}", headers=headers)
    assert ota_res.status_code == 403
    assert "revoked or decommissioned" in ota_res.json()["detail"].lower()

    # 8. Delete device permanently
    del_res = client.delete(f"/api/v1/devices/{device_id}", headers=headers)
    assert del_res.status_code == 200

    # Verify device no longer exists
    get_res = client.get(f"/api/v1/devices/{device_id}", headers=headers)
    assert get_res.status_code == 404

def test_device_config_update_sync_contract(client):
    admin_token = get_admin_token(client)
    headers = {"Authorization": f"Bearer {admin_token}"}

    device_id = "esp32s3-cfg-sync-node"

    # 1. Register device
    reg_res = client.post("/api/v1/devices/register", json={
        "device_id": device_id,
        "name": "Sync Contract Node",
        "registration_token": "smartgrow-device-enrollment-secret-2026",
        "firmware_version": "1.0.0",
        "model_version": "v1.0"
    })
    assert reg_res.status_code in [200, 201]

    # 2. Check initial config state
    cfg_res = client.get(f"/api/v1/devices/{device_id}/config", headers=headers)
    assert cfg_res.status_code == 200
    cfg_data = cfg_res.json()
    assert cfg_data["operating_mode"] == "AUTO"
    assert cfg_data["is_synced"] is True

    # 3. Update configuration to SAFE mode: PUT /api/v1/devices/{id}/config
    # Contract: Backend marks is_synced = False and enqueues CONFIG_UPDATE command
    update_res = client.put(f"/api/v1/devices/{device_id}/config", json={
        "operating_mode": "SAFE",
        "telemetry_interval_s": 10,
        "control_loop_interval_s": 2
    }, headers=headers)
    assert update_res.status_code == 200
    updated_cfg = update_res.json()
    assert updated_cfg["operating_mode"] == "SAFE"
    assert updated_cfg["is_synced"] is False

    # 4. Device polls pending commands -> MUST receive CONFIG_UPDATE command
    poll_res = client.get(f"/api/v1/devices/{device_id}/poll-commands")
    assert poll_res.status_code == 200
    pending_cmds = poll_res.json()
    assert len(pending_cmds) >= 1
    cfg_cmd = [c for c in pending_cmds if c["command_type"] == "CONFIG_UPDATE"][0]
    payload = json.loads(cfg_cmd["payload_json"])
    assert int(payload["param_val"]) == 4  # CONTROL_MODE_SAFE = 4
    cmd_id = cfg_cmd["command_id"]

    # 5. While unacknowledged, config must remain is_synced = False
    check_cfg = client.get(f"/api/v1/devices/{device_id}/config", headers=headers)
    assert check_cfg.json()["is_synced"] is False

    # 6. Device applies config and sends ACK with status "completed"
    ack_res = client.post(f"/api/v1/devices/{device_id}/commands/ack", json={
        "command_id": cmd_id,
        "status": "completed",
        "message": "ControlMode updated to 4 and saved to NVS"
    })
    assert ack_res.status_code == 200

    # 7. Backend contract: is_synced is now confirmed True!
    synced_cfg = client.get(f"/api/v1/devices/{device_id}/config", headers=headers)
    assert synced_cfg.status_code == 200
    assert synced_cfg.json()["is_synced"] is True
    assert synced_cfg.json()["operating_mode"] == "SAFE"

    # 8. Negative test: Rejected command does NOT mark is_synced = True
    client.put(f"/api/v1/devices/{device_id}/config", json={
        "operating_mode": "HYBRID"
    }, headers=headers)
    
    poll_res2 = client.get(f"/api/v1/devices/{device_id}/poll-commands")
    cfg_cmd2 = [c for c in poll_res2.json() if c["command_type"] == "CONFIG_UPDATE"][0]
    
    client.post(f"/api/v1/devices/{device_id}/commands/ack", json={
        "command_id": cfg_cmd2["command_id"],
        "status": "rejected",
        "message": "NVS write failed or mode rejected"
    })

    unsynced_cfg = client.get(f"/api/v1/devices/{device_id}/config", headers=headers)
    assert unsynced_cfg.json()["is_synced"] is False

    # Cleanup
    client.delete(f"/api/v1/devices/{device_id}", headers=headers)
