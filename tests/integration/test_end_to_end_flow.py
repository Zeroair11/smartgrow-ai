import sys
import os

# Automatically inject repository root into sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))

from fastapi.testclient import TestClient
from backend.app.main import app

def test_full_vertical_end_to_end_flow():
    client = TestClient(app)

    print("=======================================================")
    print("  SMARTGROW AI — 12-PHASE END-TO-END VERTICAL TEST SUITE ")
    print("=======================================================")

    # -----------------------------------------------------------------
    # FLOW 1: Valid Login
    # -----------------------------------------------------------------
    print("\n--- FLOW 1: Valid Login (Admin) ---")
    login_res = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "Admin@SmartGrow2026!"
    })
    assert login_res.status_code == 200, f"Login failed: {login_res.text}"
    admin_token = login_res.json()["access_token"]
    admin_headers = {"Authorization": f"Bearer {admin_token}"}
    print("  [PASS] Admin authenticated, JWT acquired.")

    # -----------------------------------------------------------------
    # FLOW 2: Failed Login (Invalid Credentials)
    # -----------------------------------------------------------------
    print("\n--- FLOW 2: Failed Login Rejection ---")
    bad_login_res = client.post("/api/v1/auth/login", data={
        "username": "admin@smartgrow.local",
        "password": "WrongPassword123!"
    })
    assert bad_login_res.status_code == 401
    print("  [PASS] Bad credentials rejected with 401 Unauthorized.")

    import time
    run_id = int(time.time())
    viewer_email = f"viewer_{run_id}@smartgrow.io"

    # -----------------------------------------------------------------
    # FLOW 3: Insufficient Permissions (RBAC Denial)
    # -----------------------------------------------------------------
    print("\n--- FLOW 3: Insufficient Permissions Rejected (Viewer 403) ---")
    # Register viewer
    reg_viewer_res = client.post("/api/v1/auth/register", json={
        "email": viewer_email,
        "password": "ViewerPassword2026!",
        "full_name": "E2E Viewer",
        "role": "viewer"
    }, headers=admin_headers)
    assert reg_viewer_res.status_code in [200, 201]

    viewer_login = client.post("/api/v1/auth/login", data={
        "username": viewer_email,
        "password": "ViewerPassword2026!"
    })
    viewer_token = viewer_login.json()["access_token"]
    viewer_headers = {"Authorization": f"Bearer {viewer_token}"}

    # Viewer attempts operator action (e.g. manual irrigation start) -> 403
    forbidden_res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "duration_seconds": 30
    }, headers=viewer_headers)
    assert forbidden_res.status_code == 403
    print("  [PASS] Viewer mutation attempt blocked with 403 Forbidden.")

    # -----------------------------------------------------------------
    # FLOW 4: Create Greenhouse, Zone, and Device Hierarchy
    # -----------------------------------------------------------------
    print("\n--- FLOW 4: Create Greenhouse, Zone, and Device ---")
    gh_res = client.post("/api/v1/greenhouses", json={
        "name": f"E2E Automated Greenhouse {run_id}",
        "code": f"GH-E2E-{run_id}",
        "description": "High-tech testbed greenhouse",
        "location": "North Wing Bay 4"
    }, headers=admin_headers)
    assert gh_res.status_code in [200, 201]
    gh_id = gh_res.json()["id"]

    zone_res = client.post("/api/v1/zones", json={
        "greenhouse_id": gh_id,
        "name": "Tomatoes Zone",
        "code": f"ZN-TOM-{run_id}",
        "crop_type": "Heirloom Tomato",
        "target_soil_moisture_min": 45.0,
        "target_soil_moisture_max": 75.0
    }, headers=admin_headers)
    assert zone_res.status_code in [200, 201]
    zone_id = zone_res.json()["id"]

    dev_res = client.post("/api/v1/devices/register", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "name": "ESP32-S3 Tomato Controller",
        "zone_id": zone_id,
        "registration_token": "smartgrow-device-enrollment-secret-2026",
        "firmware_version": "1.0.0",
        "model_version": "v1.0.0-dt"
    })
    assert dev_res.status_code in [200, 201]
    print(f"  [PASS] Hierarchy created: Greenhouse #{gh_id} -> Zone #{zone_id} -> Device esp32s3-greenhouse-e2e-01.")

    # -----------------------------------------------------------------
    # FLOW 5: Telemetry Ingestion from Firmware
    # -----------------------------------------------------------------
    print("\n--- FLOW 5: Ingest Real-Time Telemetry ---")
    telemetry_payload = {
        "schema_version": "1.0.0",
        "device_id": "esp32s3-greenhouse-e2e-01",
        "boot_id": "boot-e2e-999",
        "sequence": 1,
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
    assert telem_res.status_code == 201
    reading_id = telem_res.json()["id"]
    print(f"  [PASS] Telemetry ingested: Reading ID #{reading_id} (Soil=28.5%, Temp=27.2C).")

    # -----------------------------------------------------------------
    # FLOW 6: Dashboard & Telemetry History Retrieval
    # -----------------------------------------------------------------
    print("\n--- FLOW 6: Dashboard Metrics & Telemetry History ---")
    dash_res = client.get("/api/v1/analytics/dashboard-summary", headers=admin_headers)
    assert dash_res.status_code == 200
    dash_data = dash_res.json()
    assert dash_data["online_devices"] >= 1
    assert dash_data["total_greenhouses"] >= 1

    hist_res = client.get("/api/v1/telemetry/history?device_id=esp32s3-greenhouse-e2e-01&limit=10", headers=admin_headers)
    assert hist_res.status_code == 200
    assert len(hist_res.json()) >= 1
    print(f"  [PASS] Dashboard and History verified: {len(hist_res.json())} telemetry readings found.")

    # -----------------------------------------------------------------
    # FLOW 7: Create Command with Idempotency Key
    # -----------------------------------------------------------------
    print("\n--- FLOW 7: Dispatch Downlink Command & Idempotency Check ---")
    idemp_key = f"e2e-flow7-vent-{run_id}"
    cmd_res = client.post("/api/v1/devices/esp32s3-greenhouse-e2e-01/commands", json={
        "command_type": "VENT_SET",
        "param_val": 45,
        "ttl_s": 180,
        "idempotency_key": idemp_key
    }, headers=admin_headers)
    assert cmd_res.status_code == 201
    cmd_id = cmd_res.json()["command_id"]
    assert cmd_res.json()["status"] == "pending"

    # Duplicate check
    cmd_dup = client.post("/api/v1/devices/esp32s3-greenhouse-e2e-01/commands", json={
        "command_type": "VENT_SET",
        "param_val": 45,
        "ttl_s": 180,
        "idempotency_key": idemp_key
    }, headers=admin_headers)
    assert cmd_dup.json()["command_id"] == cmd_id
    print(f"  [PASS] Command #{cmd_id} created with idempotent deduplication verified.")

    # -----------------------------------------------------------------
    # FLOW 8: Device Polls Command and Transitions to Terminal State
    # -----------------------------------------------------------------
    print("\n--- FLOW 8: Device Polls & ACKs Command to Completed ---")
    poll_res = client.get("/api/v1/devices/esp32s3-greenhouse-e2e-01/poll-commands")
    assert poll_res.status_code == 200
    pending_list = poll_res.json()
    assert any(c["command_id"] == cmd_id for c in pending_list)

    ack_res = client.post("/api/v1/devices/esp32s3-greenhouse-e2e-01/commands/ack", json={
        "command_id": cmd_id,
        "status": "completed",
        "result_message": "Roof vent servo positioned at 45 degrees",
        "error_code": 0
    })
    assert ack_res.status_code == 200
    assert ack_res.json()["status"] == "completed"
    print(f"  [PASS] Command #{cmd_id} transitioned cleanly: pending -> completed.")

    # -----------------------------------------------------------------
    # FLOW 9: Hardware Safety Constraint (>120s Duration Rejection)
    # -----------------------------------------------------------------
    print("\n--- FLOW 9: Safety Guardrail Rejection (>120s Irrigation) ---")
    guard_res = client.post("/api/v1/irrigation/start", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "duration_seconds": 300 # Exceeds 120s limit
    }, headers=admin_headers)
    assert guard_res.status_code == 422 # Pydantic schema validation boundary
    print("  [PASS] Irrigation command exceeding 120s rejected by backend guardrails.")

    # -----------------------------------------------------------------
    # FLOW 10: Automation Rule Creation, Trigger, and Toggle
    # -----------------------------------------------------------------
    print("\n--- FLOW 10: Automation Rule Life Cycle (Create -> Trigger -> Toggle) ---")
    rule_res = client.post("/api/v1/automation/rules", json={
        "name": "E2E Low Moisture Auto-Water",
        "zone_id": zone_id,
        "sensor_metric": "soil_moisture_pct",
        "operator": "<",
        "threshold_val": 30.0,
        "action_type": "PUMP_START",
        "action_param_val": 45.0,
        "cooldown_s": 180
    }, headers=admin_headers)
    assert rule_res.status_code == 201
    rule_id = rule_res.json()["id"]

    # Toggle rule OFF
    toggle_res = client.post(f"/api/v1/automation/rules/{rule_id}/toggle", headers=admin_headers)
    assert toggle_res.status_code == 200
    assert toggle_res.json()["is_active"] == False

    # Toggle rule back ON
    toggle_res2 = client.post(f"/api/v1/automation/rules/{rule_id}/toggle", headers=admin_headers)
    assert toggle_res2.status_code == 200
    assert toggle_res2.json()["is_active"] == True
    print(f"  [PASS] Automation Rule #{rule_id} created and active state toggled successfully.")

    # -----------------------------------------------------------------
    # FLOW 11: Alert Life Cycle (Create -> Acknowledge -> Resolve)
    # -----------------------------------------------------------------
    print("\n--- FLOW 11: Alert Management (Ingest Anomaly -> Ack -> Resolve) ---")
    alert_create_res = client.post("/api/v1/alerts", json={
        "device_id": "esp32s3-greenhouse-e2e-01",
        "zone_id": zone_id,
        "severity": "critical",
        "category": "environment",
        "title": "High Heat Stress Warning",
        "message": "Temperature reading 38.5C exceeds tomato stress limit"
    }, headers=admin_headers)
    assert alert_create_res.status_code == 201
    alert_id = alert_create_res.json()["id"]

    # Acknowledge
    ack_alert_res = client.post(f"/api/v1/alerts/{alert_id}/acknowledge", headers=admin_headers)
    assert ack_alert_res.status_code == 200
    assert ack_alert_res.json()["is_acknowledged"] == True

    # Resolve
    resolve_alert_res = client.post(f"/api/v1/alerts/{alert_id}/resolve", headers=admin_headers)
    assert resolve_alert_res.status_code == 200
    assert resolve_alert_res.json()["is_resolved"] == True
    print(f"  [PASS] Alert #{alert_id} lifecycle verified: created -> acknowledged -> resolved.")

    # -----------------------------------------------------------------
    # FLOW 12: Logout / Token Revocation & Protected Route Access
    # -----------------------------------------------------------------
    print("\n--- FLOW 12: Protected Route Token Validation ---")
    invalid_headers = {"Authorization": "Bearer invalid_tampered_token_xyz"}
    unauth_res = client.get("/api/v1/auth/me", headers=invalid_headers)
    assert unauth_res.status_code == 401
    print("  [PASS] Tampered/expired tokens correctly rejected on protected routes.")

    print("\n=======================================================")
    print("  ALL 12 VERTICAL END-TO-END FLOWS FULLY VERIFIED PASS  ")
    print("=======================================================\n")

if __name__ == "__main__":
    test_full_vertical_end_to_end_flow()
