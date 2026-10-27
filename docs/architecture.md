# SmartGrow AI — Architecture & System Design Document

## 1. System Philosophy & Objectives
SmartGrow AI is an industrial-grade greenhouse management platform that seamlessly unifies embedded edge intelligence, hard real-time safety guardrails, cloud telemetry ingestion, and modern web operations.

Key Design Tenets:
- **Local Autonomy:** The ESP32-S3 edge controller must continue enforcing safety policies and plant protection even if Internet or backend connectivity is completely severed.
- **Safety Interlock Superiority:** Neither remote cloud commands nor Edge AI inference recommendations are permitted to override hard firmware safety interlocks (maximum continuous pump runtime and mandatory cooldown cycles).
- **Asynchronous Command Lifecycle:** Issuing an HTTP command request is never assumed to be execution confirmation; execution is only confirmed when the physical edge device polls, checks safety, actuates, and issues a cryptographically authenticated ACK.
- **Deterministic TinyML:** Machine learning models running on the ESP32-S3 controller must execute with zero dynamic memory allocation (0 bytes heap), guaranteeing immunity against memory fragmentation and crashes.

---

## 2. End-to-End System Architecture

```
+-----------------------------------------------------------------------------------+
|                           ESP32-S3 DEVMODULE N16R8                                 |
|                                                                                   |
|  [Sensors & ADC]               [Edge Computing]             [Actuators & Relays]  |
|  - Soil Moisture (GPIO 1) ---> Sensor Manager               - Pump Relay (GPIO 10)|
|  - LDR Light (GPIO 2)     ---> Sensor Calibration           - Vent Servo (GPIO 11)|
|  - DHT11 Temp/Hum (GPIO 4)---> [TinyML Decision Tree]       - Buzzer (GPIO 12)    |
|                                       |                     - Status LED (GPIO 21)|
|                                       v                               ^           |
|                            [Firmware Safety Manager] -----------------+           |
|                            - Max Run Cutoff: 120s                                 |
|                            - Min Cooldown: 180s                                   |
|                            - Sensor Fault Trip                                    |
|                                       |                                           |
|                            [Telemetry & Command Client]                           |
+-----------------------------------------------------------------------------------+
                                        |  ^
                           HTTP / REST  |  |  HTTP Polling
                                        v  |
+-----------------------------------------------------------------------------------+
|                           FASTAPI BACKEND PLATFORM                                |
|                                                                                   |
|  - Versioned API: /api/v1 (Auth, Devices, Telemetry, Irrigation, Automation, etc.)|
|  - Security: JWT HS256, Password Hashing, Role-Based Access Control               |
|  - Command Lifecycle: Pending -> Polled -> Executing -> Completed / Failed / Exp  |
|  - Automation Engine: Threshold monitoring, cooldowns, trigger activity logging   |
|  - Background Monitor: 15s asynchronous sweep for offline devices & expired cmds  |
|  - Relational Database: SQLAlchemy ORM (SQLite for local dev, PostgreSQL ready)   |
+-----------------------------------------------------------------------------------+
                                        |  ^
                               REST API |  | JSON Requests / Responses
                                        v  |
+-----------------------------------------------------------------------------------+
|                           VUE 3 FRONTEND WEB APPLICATION                          |
|                                                                                   |
|  - Industrial IoT Dashboard: High-observability dark theme (Tailwind CSS)         |
|  - Pinia State Management: Token persistence, reactive state, auto-reauth        |
|  - 12 Complete Functional Modules: Dashboard, Live Monitoring, Greenhouses,       |
|    Devices, Edge AI Center, Automation Rules, Irrigation, Alerts, Schedule,      |
|    Analytics, System Settings, Users & Audit Trail                                |
+-----------------------------------------------------------------------------------+
```

---

## 3. Command Lifecycle State Machine

Each actuator downlink command traverses an explicit state machine:

```
                  +--------------+
                  |   PENDING    |
                  +--------------+
                    /     |    \
     (TTL Expired) /      |     \ (Polled by Device)
                  v       |      v
          +---------+     |    +----------------+
          | EXPIRED |     |    |  ACKNOWLEDGED  |
          +---------+     |    +----------------+
                          |      /            \
          (Offline/Fault) |     / (Pass)       \ (Safety Trip)
                          v    v                v
                  +-------------+         +------------+
                  |  COMPLETED  |         |  REJECTED  |
                  +-------------+         +------------+
```

1. **Pending:** Command created by Operator or Automation Rule with a defined TTL (default 300 seconds).
2. **Expired:** If the device is offline or fails to poll before `expires_at`, backend marks the command expired.
3. **Acknowledged:** Device polls `/devices/{id}/poll-commands` and receives the command.
4. **Safety Verification:** Edge `safety_manager_can_start_pump` checks cooldown and sensor validity.
5. **Completed:** Actuator successfully driven; device sends ACK payload with `status="completed"`.
6. **Rejected:** If safety conditions are violated, device sends ACK with `status="rejected"` and error code.

---

## 4. Edge AI & Safety Priority Interlock

When the TinyML Decision Tree predicts `AI_CLASS_NEED_WATER`:
- Firmware invokes `safety_manager_can_start_pump()`.
- If a cooldown is active or sensors report invalid numbers, the Safety Manager **overrides** the recommendation.
- The event is recorded in the telemetry packet as `safety_override=true`, action=`SAFETY_OVERRIDE_BLOCK_PUMP`.
- The pump remains physically unpowered.

---

## 5. FreeRTOS Task Architecture & Network Concurrency

The ESP32-S3 firmware enforces strict temporal isolation across dual Xtensa LX7 cores:

| Task Name | Core | Priority | Stack Size | Periodicity | Responsibilities |
|---|---|---|---|---|---|
| `safety_sensor_task` | Core 0 | High (5) | 4,096 B | 100 ms | Hard real-time safety watchdog (`safety_manager_tick`), periodic ADC/sensor acquisition (2000 ms), TinyML Decision Tree inference (`edge_ai_infer`), thread-safe telemetry queuing. |
| `network_uplink_task` | Core 1 | Med (3) | 6,144 B | 500 ms poll | Wi-Fi exponential backoff tick, device enrollment (`POST /devices/register`), periodic telemetry upload (`POST /telemetry`, 5000 ms), downlink command polling (`GET /devices/{id}/poll-commands`, 3000 ms), immediate ACK dispatch. |
| `serial_hil_task` | Core 0 | Low (2) | 2,048 B | 100 ms | UART0 serial protocol responder (responds to `PING` with `PONG ESP32-S3 READY`) for physical test bench integration. |

### Network Resilience & Safety Invariant
- **Safety Loop Independence:** Network latency, DNS resolution delays, HTTP timeouts, and Wi-Fi disconnects execute exclusively on Core 1 and can **never** block or delay Core 0's 100 ms `safety_manager_tick` loop.
- **Actuator Power State:** If Wi-Fi is lost, the pump relay defaults to its hardware-safe state (`!RELAY_PUMP_ACTIVE_LEVEL`). Network reconnection never causes previous commands to be re-executed.

---

## 6. Centralized Firmware Configuration Architecture

The firmware enforces a single source of truth for all operational, hardware, networking, safety, and sensor configuration parameters via `firmware_config.h` and `config_defaults.h`.

### 6.1. Configuration Hierarchy & Precedence
1. **Developer / Environment Overrides:** `firmware/include/config_local.h` (git-ignored, template in `config_local.h.example`).
2. **Compile-Time Baseline Defaults:** `firmware/include/config_defaults.h`.
3. **Runtime Master Configuration Store:** Managed by `firmware_config_init()`, validated by `firmware_config_validate()`, and queried via `firmware_config_get()`.

### 6.2. Configuration Groups
- **Device Identity:** Unique `device_id` (fallback or provisioning), `device_name`, `firmware_version`, `hardware_rev`, `schema_version`, and unique `boot_id`.
- **Wi-Fi Connectivity:** `ssid`, `password`, `max_retries`, exponential backoff parameters (`reconnect_initial_delay_ms`, `reconnect_max_delay_ms`, `reconnect_backoff_factor`), and `connection_timeout_ms`.
- **Backend & Network:** `host`, `port`, `base_path`, `telemetry_endpoint`, `register_endpoint`, `poll_commands_endpoint`, `ack_endpoint`, `telemetry_interval_ms` (5000ms), `command_poll_interval_ms` (3000ms), and `http_timeout_ms` (5000ms).
- **DHT11 Air Sensor:** `pin_data` (GPIO 4), `sample_interval_ms` (2000ms), `read_timeout_ms` (50ms), `max_consecutive_errors` (3), `min_temp_c` (0.0°C), `max_temp_c` (50.0°C), `min_humidity_pct` (20.0%), and `max_humidity_pct` (90.0%).
- **Soil & Light Sensors:** ADC pin channels, dry/wet ADC calibration bounds (`adc_dry_value`, `adc_wet_value`), plausible thresholds (`adc_min_plausible`, `adc_max_plausible`), and LDR scaling.
- **Actuators & Safety:** Active-low pump relay configuration (`relay_active_level=0`, `relay_inactive_level=1`), servo PWM parameters, buzzer/LED pins, `max_pump_runtime_s` (120s), `min_cooldown_s` (180s), and `require_sensor_validity` (true).
- **Edge AI & TinyML:** Model version, feature scaling factors, confidence thresholds, and inference periodicity.
- **OTA & Diagnostics:** OTA update URLs, poll intervals, serial baud rate (115200), and log level.

### 6.3. Secret Isolation & Logging Security
- Mặc định credentials và secret (`wifi_password`, `device_token`) được mask hoàn toàn thành `********` khi log ra UART thông qua `firmware_config_log_safe()`.
- File `config_local.h` chứa thông tin nhạy cảm được cấu hình trong `.gitignore`, không bao giờ commit vào Git repository.


