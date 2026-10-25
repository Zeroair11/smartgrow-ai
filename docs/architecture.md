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
|  - DHT22 Temp/Hum (GPIO 4)---> [TinyML Decision Tree]       - Buzzer (GPIO 12)    |
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
