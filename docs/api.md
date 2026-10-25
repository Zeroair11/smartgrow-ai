# SmartGrow AI — REST API Specification (v1)

Base URL: `http://localhost:8000/api/v1`  
Interactive Swagger Docs: `http://localhost:8000/api/docs`  
Interactive ReDoc: `http://localhost:8000/api/redoc`

---

## 1. Authentication (`/auth`)

### `POST /auth/login`
- **Desc:** Authenticate operator/admin and acquire JWT access token.
- **Form Data:** `username` (email), `password`.
- **Response (200 OK):**
  ```json
  {
    "access_token": "eyJhbGciOi...",
    "token_type": "bearer",
    "role": "admin",
    "user_id": 1,
    "email": "admin@smartgrow.local"
  }
  ```

### `GET /auth/me`
- **Header:** `Authorization: Bearer <token>`
- **Response (200 OK):** Returns current user profile.

### `POST /auth/register` (Admin Only)
- **Body:** `{ "email": "op@smartgrow.local", "password": "...", "full_name": "...", "role": "operator" }`

---

## 2. Greenhouses & Cultivation Zones (`/greenhouses`, `/zones`)

### `GET /greenhouses`
- **Response (200 OK):** Array of registered greenhouse facilities.

### `POST /greenhouses`
- **Body:** `{ "name": "Greenhouse Alpha", "code": "GH-01", "location": "Sector 4" }`

### `GET /zones?greenhouse_id={id}`
- **Response (200 OK):** Array of cultivation zones with moisture & temperature thresholds.

### `POST /zones`
- **Body:**
  ```json
  {
    "greenhouse_id": 1,
    "name": "Zone A - Tomatoes",
    "code": "Z-01",
    "crop_type": "Cherry Tomato",
    "target_soil_moisture_min": 40.0,
    "target_soil_moisture_max": 70.0,
    "target_temp_min": 18.0,
    "target_temp_max": 32.0
  }
  ```

---

## 3. Devices & Fleet (`/devices`)

### `POST /devices/register`
- **Desc:** Hardware registration invoked by ESP32-S3 or administrator.
- **Body:**
  ```json
  {
    "device_id": "esp32s3-unit-01",
    "name": "Zone 1 Edge Node",
    "registration_token": "smartgrow-device-enrollment-secret-2026",
    "firmware_version": "1.0.0",
    "model_version": "v1.0.0-dt"
  }
  ```

### `GET /devices`
- **Response (200 OK):** List of controllers with status (`online`/`offline`), last seen timestamp, and firmware version.

### `GET /devices/{device_id}/poll-commands`
- **Desc:** Invoked by firmware on periodic poll to receive pending downlink commands.
- **Response (200 OK):** Array of pending `DeviceCommand` objects.

### `POST /devices/{device_id}/commands/ack`
- **Desc:** Invoked by firmware after command execution.
- **Body:**
  ```json
  {
    "command_id": "cmd-5968621ba198",
    "status": "completed",
    "result_message": "Pump run completed for 30s",
    "error_code": 0
  }
  ```

---

## 4. Telemetry (`/telemetry`)

### `POST /telemetry`
- **Desc:** High-frequency uplink endpoint called by ESP32-S3 controller.
- **Body:**
  ```json
  {
    "schema_version": "1.0.0",
    "device_id": "esp32s3-unit-01",
    "boot_id": "boot-001",
    "sequence": 42,
    "soil_moisture_pct": 34.2,
    "temperature_c": 27.5,
    "humidity_pct": 68.0,
    "light_level_pct": 82.0,
    "ai_class": 1,
    "ai_confidence": 0.95,
    "pump_state": false,
    "operating_mode": "AUTO",
    "health_status": "HEALTHY"
  }
  ```

### `GET /telemetry/latest?device_id={id}`
- **Response (200 OK):** Most recent sensor reading.

### `GET /telemetry/export-csv`
- **Response (200 OK):** Streams downloadable `smartgrow_telemetry.csv`.

---

## 5. Irrigation & Actuation (`/irrigation`)

### `POST /irrigation/start`
- **Desc:** Manual pump start with hard guardrail validation.
- **Body:** `{ "device_id": "esp32s3-unit-01", "duration_seconds": 30 }`
- **Validation:** `duration_seconds` must be $\le 120$ seconds.

### `POST /irrigation/stop`
- **Desc:** Immediate emergency pump cutoff command.

### `GET /irrigation/events`
- **Response (200 OK):** Historical irrigation log with duration, estimated volume, and stop reasons.

---

## 6. Automation Rules (`/automation`)

### `GET /automation/rules`
- **Response (200 OK):** Array of automation rules.

### `POST /automation/rules`
- **Body:**
  ```json
  {
    "name": "Dry Soil Auto Water",
    "sensor_metric": "soil_moisture_pct",
    "operator": "<",
    "threshold_val": 30.0,
    "action_type": "PUMP_START",
    "action_param_val": 30.0,
    "cooldown_s": 300
  }
  ```

---

## 7. Edge AI, Alerts, Analytics & System
- `/edge-ai/inferences`: Historical TinyML inference classifications.
- `/alerts`: Incident list with severity filters and `acknowledge`/`resolve` endpoints.
- `/analytics/dashboard-summary`: System-wide KPI summary.
- `/system/audit-logs`: Administrative audit trail.
- `/ota/releases`: Firmware releases and over-the-air binary distribution.
