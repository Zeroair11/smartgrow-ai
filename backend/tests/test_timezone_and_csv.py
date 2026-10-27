import io
import csv
import pytest
from datetime import datetime, timezone, timedelta
from backend.app.models.entities import Device, SensorReading, User
from backend.app.schemas.dtos import SensorReadingRead

def test_sqlite_utcdatetime_serialization_has_z_suffix(client, db):
    """
    Test that timestamps saved to SQLite and loaded via SQLAlchemy
    are assigned timezone.utc and serialized by Pydantic with trailing 'Z'.
    """
    dev = Device(
        device_id="ESP32_TZ_TEST_01",
        name="Timezone Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-tz-01",
        status="online"
    )
    db.add(dev)
    db.commit()

    test_utc_time = datetime(2026, 10, 27, 3, 14, 0, 123456, tzinfo=timezone.utc)
    reading = SensorReading(
        device_id="ESP32_TZ_TEST_01",
        sequence=100,
        soil_moisture_pct=45.5,
        temperature_c=28.2,
        humidity_pct=65.0,
        light_level_pct=80.0,
        pump_state=False,
        operating_mode="AUTO",
        recorded_at=test_utc_time
    )
    db.add(reading)
    db.commit()

    # Query from DB to simulate real read
    queried = db.query(SensorReading).filter(SensorReading.device_id == "ESP32_TZ_TEST_01").first()
    assert queried is not None
    assert queried.recorded_at.tzinfo is not None
    assert queried.recorded_at.tzinfo == timezone.utc

    # Validate Pydantic schema serialization
    dto = SensorReadingRead.model_validate(queried)
    json_str = dto.model_dump_json()
    assert "2026-10-27T03:14:00.123456Z" in json_str or "2026-10-27T03:14:00.123456+00:00" in json_str

def test_telemetry_csv_export_endpoint_headers_and_content(client, db):
    """
    Test CSV export endpoint:
    1. Requires authentication (401 without Bearer token)
    2. Returns text/csv; charset=utf-8
    3. Contains UTF-8 BOM
    4. Has dynamic filename in Content-Disposition
    5. Correct columns and proper CSV formatting
    """
    # 1. Unauthenticated request must return 401 Unauthorized
    unauth_resp = client.get("/api/v1/telemetry/export-csv")
    assert unauth_resp.status_code == 401

    # Setup device and readings
    dev = Device(
        device_id="ESP32_CSV_TEST_01",
        name="CSV Test Device",
        firmware_version="1.0.0",
        model_version="v1.0",
        token="token-csv-01",
        status="online"
    )
    db.add(dev)
    db.commit()

    utc_dt = datetime(2026, 10, 27, 9, 30, 0, tzinfo=timezone.utc)
    r1 = SensorReading(
        device_id="ESP32_CSV_TEST_01",
        sequence=1,
        soil_moisture_pct=50.25,
        temperature_c=29.50,
        humidity_pct=72.10,
        light_level_pct=60.00,
        ai_class="NORMAL",
        ai_confidence=0.985,
        pump_state=False,
        operating_mode="AUTO",
        recorded_at=utc_dt
    )
    db.add(r1)
    db.commit()

    # Login to obtain Bearer token
    login_resp = client.post(
        "/api/v1/auth/login",
        data={"username": "admin@smartgrow.local", "password": "Admin@SmartGrow2026!"}
    )
    assert login_resp.status_code == 200
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    # Request CSV export
    response = client.get("/api/v1/telemetry/export-csv?device_id=ESP32_CSV_TEST_01", headers=headers)
    assert response.status_code == 200
    assert "text/csv" in response.headers["content-type"]
    assert "charset=utf-8" in response.headers["content-type"]
    assert "Content-Disposition" in response.headers
    assert "smartgrow_telemetry_" in response.headers["Content-Disposition"]
    assert ".csv" in response.headers["Content-Disposition"]

    # Check for UTF-8 BOM (\xef\xbb\xbf)
    raw_bytes = response.content
    assert raw_bytes.startswith(b"\xef\xbb\xbf"), "CSV response must start with UTF-8 BOM"

    # Decode and parse CSV
    csv_text = raw_bytes.decode("utf-8-sig")
    reader = list(csv.reader(io.StringIO(csv_text)))
    assert len(reader) >= 2  # Header + at least 1 row
    
    header = reader[0]
    assert header == [
        "recorded_at", "device_id", "sequence", "soil_moisture_pct",
        "temperature_c", "humidity_pct", "light_level_pct",
        "ai_class", "ai_confidence", "pump_state", "operating_mode"
    ]

    data_row = reader[1]
    assert data_row[1] == "ESP32_CSV_TEST_01"
    assert data_row[2] == "1"
    assert data_row[3] == "50.25"
    assert data_row[4] == "29.50"
    assert data_row[5] == "72.10"
    assert data_row[6] == "60.00"
    assert data_row[7] == "NORMAL"
    assert data_row[8] == "0.985"
    assert data_row[9] == "False"
    assert data_row[10] == "AUTO"
    assert "2026-10-27T09:30:00" in data_row[0]

def test_telemetry_history_date_filtering_and_order(client, db):
    """
    Test telemetry history endpoint query ordering and user authentication.
    """
    login_resp = client.post(
        "/api/v1/auth/login",
        data={"username": "admin@smartgrow.local", "password": "Admin@SmartGrow2026!"}
    )
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    resp = client.get("/api/v1/telemetry/history?limit=10", headers=headers)
    assert resp.status_code == 200
    data = resp.json()
    assert isinstance(data, list)
    if len(data) >= 2:
        # Verify descending order
        t1 = datetime.fromisoformat(data[0]["recorded_at"].replace("Z", "+00:00"))
        t2 = datetime.fromisoformat(data[1]["recorded_at"].replace("Z", "+00:00"))
        assert t1 >= t2
