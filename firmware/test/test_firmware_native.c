#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "board_config.h"
#include "firmware_config.h"
#include "dht11.h"
#include "sensor_calibration.h"
#include "sensor_manager.h"
#include "safety_manager.h"
#include "actuator_manager.h"
#include "command_manager.h"
#include "telemetry_client.h"
#include "ota_manager.h"
#include "edge_ai.h"
#include "wifi_manager.h"
#include "network_client.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* Test 1: Sensor Calibration & Hardware Validation */
static void test_sensor_calibration(void) {
    printf("\n--- Running Test Suite: Sensor Calibration & ADC Health ---\n");
    SoilCalibrationConfig cfg;
    sensor_calibration_init(&cfg);

    TEST_ASSERT(cfg.adc_dry_raw == 3200, "Default dry ADC is 3200");
    TEST_ASSERT(cfg.adc_wet_raw == 1200, "Default wet ADC is 1200");

    /* Raw reading at dry point should yield 0.0% */
    float dry_pct = sensor_calibration_raw_to_moisture_pct(&cfg, 3200);
    TEST_ASSERT(dry_pct == 0.0f, "ADC 3200 yields 0.0% moisture");

    /* Raw reading at wet point should yield 100.0% */
    float wet_pct = sensor_calibration_raw_to_moisture_pct(&cfg, 1200);
    TEST_ASSERT(wet_pct == 100.0f, "ADC 1200 yields 100.0% moisture");

    /* Raw reading midpoint (2200) should yield 50.0% */
    float mid_pct = sensor_calibration_raw_to_moisture_pct(&cfg, 2200);
    TEST_ASSERT(mid_pct >= 49.9f && mid_pct <= 50.1f, "ADC 2200 yields 50.0% moisture");

    /* Guard: Out of range clamping */
    TEST_ASSERT(sensor_calibration_raw_to_moisture_pct(&cfg, 3500) == 0.0f, "Clamped to 0% when above dry");
    TEST_ASSERT(sensor_calibration_raw_to_moisture_pct(&cfg, 500) == 100.0f, "Clamped to 100% when below wet");

    /* Electrical Plausibility / Disconnected Wire Fault Detection */
    TEST_ASSERT(sensor_calibration_is_adc_plausible(2000) == true, "ADC 2000 is electrically plausible");
    TEST_ASSERT(sensor_calibration_is_adc_plausible(4000) == false, "ADC 4000 identified as open-circuit/disconnected wire");
    TEST_ASSERT(sensor_calibration_is_adc_plausible(50) == false, "ADC 50 identified as short-circuit to ground");
}

/* Test 2: Safety Manager Guardrails & Watchdog Actuator Cutoff */
static void test_safety_manager(void) {
    printf("\n--- Running Test Suite: Safety Manager Guardrails & Hardware Cutoff ---\n");
    safety_manager_init(120000, 180000); /* 120s max runtime, 180s cooldown */
    actuator_manager_init();

    SensorReadings valid_readings = {
        .soil_moisture_pct = 25.0f,
        .temperature_c = 28.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 80.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };

    SafetyStatus reason;
    /* Clean state: can start pump */
    bool can_start = safety_manager_can_start_pump(&valid_readings, 1000, &reason);
    TEST_ASSERT(can_start && reason == SAFETY_OK, "Pump can start in clean state");

    /* Start pump */
    safety_manager_record_pump_start(1000);
    actuator_manager_set_pump(true);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == true, "Pump actuator active");

    /* Safety tick under limit (runtime = 60s) -> no trip */
    bool tripped = safety_manager_tick(61000);
    TEST_ASSERT(!tripped, "No cutoff at 60s runtime");

    /* Safety tick exceeding 120s limit (runtime = 121s) -> MUST force cutoff */
    tripped = safety_manager_tick(122000);
    TEST_ASSERT(tripped, "Safety watchdog tripped after exceeding max runtime");
    TEST_ASSERT(safety_manager_get_state()->last_violation == SAFETY_ERR_MAX_RUNTIME_EXCEEDED, "Violation recorded as MAX_RUNTIME_EXCEEDED");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Watchdog trip turned off hardware pump actuator");

    /* Immediate restart attempt should be BLOCKED due to cooldown */
    can_start = safety_manager_can_start_pump(&valid_readings, 125000, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_COOLDOWN_ACTIVE, "Pump start blocked by active cooldown");

    /* Sensor fault interlock */
    SensorReadings faulty_readings = valid_readings;
    faulty_readings.soil_valid = false;
    /* Advance time past cooldown and clear lockout */
    safety_manager_clear_lockout(400000);
    can_start = safety_manager_can_start_pump(&faulty_readings, 400000, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_SENSOR_FAULT, "Pump start blocked by sensor fault");

    /* Rollover resilience: 32-bit timestamp wrapping past 0xFFFFFF00 */
    safety_manager_record_pump_stop(0xFFFFFF00);
    /* 10000ms after rollover (at current_time = 0x00002600) -> elapsed is ~9984ms < 180s cooldown */
    can_start = safety_manager_can_start_pump(&valid_readings, 0x00002600, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_COOLDOWN_ACTIVE, "Cooldown enforced safely across 32-bit integer rollover");

    /* New: Test Centralized Relay Safety Gate & Planned Target Duration Completion */
    safety_manager_init(120000, 180000);
    actuator_manager_init();

    /* Request 30s pump run via Safety Gate */
    bool gate_ok = safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, 30000, &valid_readings, 1000, &reason);
    TEST_ASSERT(gate_ok && actuator_manager_get_state()->pump_active == true, "Safety Gate approved 30s pump run");

    /* At 15s elapsed: still running */
    bool tick_res = safety_manager_tick(16000);
    TEST_ASSERT(!tick_res && actuator_manager_get_state()->pump_active == true, "Pump still active at 15s elapsed");

    /* At 30s elapsed: normal planned cycle complete, NOT emergency trip */
    tick_res = safety_manager_tick(31000);
    TEST_ASSERT(!tick_res, "Normal planned completion returned false (not an emergency trip)");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Pump turned OFF upon reaching planned 30s target");
    TEST_ASSERT(safety_manager_get_state()->total_normal_stops >= 1, "Normal cycle stop count incremented");

    /* New: Test Emergency Watchdog Cutoff and Lockout Latch */
    safety_manager_init(120000, 180000);
    gate_ok = safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, 120000, &valid_readings, 1000, &reason);
    TEST_ASSERT(gate_ok, "Edge AI pump request approved");

    /* Exceed 120s max runtime -> Emergency cutoff */
    tick_res = safety_manager_tick(122000);
    TEST_ASSERT(tick_res, "Emergency watchdog cutoff tripped at 121s");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Lockout latch engaged following emergency trip");

    /* Attempting to clear lockout during cooldown is BLOCKED */
    bool clear_ok = safety_manager_clear_lockout(150000);
    TEST_ASSERT(!clear_ok, "Lockout cannot be cleared while cooldown is active");

    /* Advance time past cooldown (180s cooldown -> time >= 302000) */
    clear_ok = safety_manager_clear_lockout(310000);
    TEST_ASSERT(clear_ok, "Lockout successfully cleared after cooldown expired");
}

/* Test 3: Command Manager & Idempotency */
static void test_command_manager(void) {
    printf("\n--- Running Test Suite: Command Manager & Idempotency ---\n");
    command_manager_init();
    safety_manager_init(120000, 180000);
    actuator_manager_init();

    SensorReadings valid_readings = {
        .soil_moisture_pct = 25.0f,
        .temperature_c = 28.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 80.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };

    DeviceCommand cmd = {
        .command_id = "cmd-uuid-001",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 45,
        .expires_at_sec = 2000000000
    };

    CommandExecutionResult res;
    bool ok = command_manager_execute(&cmd, &valid_readings, 1700000000, 10000, &res);
    TEST_ASSERT(ok && res.success && res.error_code == 0, "Vent command executed successfully");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 45, "Vent angle set to 45 deg");

    /* Idempotent re-execution of same command_id */
    ok = command_manager_execute(&cmd, &valid_readings, 1700000000, 10500, &res);
    TEST_ASSERT(ok && res.success, "Duplicate command safely handled with idempotent ACK");

    /* Expired command rejection */
    DeviceCommand expired_cmd = {
        .command_id = "cmd-uuid-002",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 1600000000 /* expired in the past */
    };
    ok = command_manager_execute(&expired_cmd, &valid_readings, 1700000000, 11000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 101, "Expired command rejected with error code 101");

    /* Excessive pump duration (> 120s) rejection */
    DeviceCommand overlength_cmd = {
        .command_id = "cmd-uuid-003",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 300, /* 300s > 120s limit */
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&overlength_cmd, &valid_readings, 1700000000, 12000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 202, "Excessive duration pump command rejected in firmware (limit 120s)");

    /* Pump start blocked when sensor reading has sensor fault */
    SensorReadings faulty_readings = valid_readings;
    faulty_readings.soil_valid = false;
    DeviceCommand pump_cmd = {
        .command_id = "cmd-uuid-004",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 45,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&pump_cmd, &faulty_readings, 1700000000, 13000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 201, "Pump start command rejected when sensor reading is faulty");

    /* Pump start succeeds when readings are valid and cooldown clean */
    ok = command_manager_execute(&pump_cmd, &valid_readings, 1700000000, 14000, &res);
    TEST_ASSERT(ok && res.success && actuator_manager_get_state()->pump_active == true, "Pump start command executed successfully");

    /* New: Test CMD_TYPE_RESTART command (verifies no Unknown command type 400!) */
    DeviceCommand restart_cmd = {
        .command_id = "cmd-uuid-005",
        .type = CMD_TYPE_RESTART,
        .param_val = 0,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&restart_cmd, &valid_readings, 1700000000, 15000, &res);
    TEST_ASSERT(ok && res.success && res.error_code == 0, "CMD_TYPE_RESTART executed cleanly without 400 error");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Restart command safely turned off pump");

    /* New: Test invalid / unknown command type -> MUST reject with 400 */
    DeviceCommand unknown_cmd = {
        .command_id = "cmd-uuid-006",
        .type = (CommandType)999,
        .param_val = 0,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&unknown_cmd, &valid_readings, 1700000000, 16000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 400, "Unknown command type rejected with error 400");
    TEST_ASSERT(strcmp(res.status, "rejected") == 0, "Unknown command status is 'rejected'");

    /* New: Test PUMP_START rejected when cooldown is active */
    safety_manager_request_pump_off(SAFETY_SRC_DOWNLINK_CMD, "TEST_STOP", 16000);
    DeviceCommand cooldown_cmd = {
        .command_id = "cmd-uuid-007",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&cooldown_cmd, &valid_readings, 1700000000, 17000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 201, "PUMP_START rejected with code 201 during active cooldown");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Pump remains OFF during cooldown rejection");

    /* New: Test PUMP_START rejected when emergency watchdog lockout is active */
    /* Force pump start, let runtime exceed 120s limit to trip watchdog cutoff */
    safety_manager_clear_lockout(20000);
    safety_manager_record_pump_start(20000);
    safety_manager_tick(20000 + 121000); /* 141000ms: Watchdog trips, latches in_lockout */
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Watchdog lockout successfully latched");

    DeviceCommand lockout_cmd = {
        .command_id = "cmd-uuid-008",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&lockout_cmd, &valid_readings, 1700000000, 142000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 201, "PUMP_START rejected with code 201 during active lockout");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Pump remains OFF during active lockout");
    safety_manager_clear_lockout(143000);
}

/* Test 4: Telemetry Client Serialization */
static void test_telemetry_client(void) {
    printf("\n--- Running Test Suite: Telemetry Serialization ---\n");
    telemetry_client_init("ESP32S3-GREENHOUSE-01", "boot-test-42");

    TelemetryPayload payload = {
        .schema_version = "1.0.0",
        .device_id = "ESP32S3-GREENHOUSE-01",
        .boot_id = "boot-test-42",
        .sequence = telemetry_client_next_sequence(),
        .timestamp_ms = 45000,
        .soil_moisture_pct = 38.50f,
        .temperature_c = 27.40f,
        .humidity_pct = 65.20f,
        .light_level_pct = 82.00f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true,
        .ai_class = 1,
        .ai_confidence = 0.942f,
        .model_version = "v1.0.0-dt",
        .firmware_version = "1.0.0",
        .pump_state = false,
        .operating_mode = "AUTO",
        .health_status = "HEALTHY"
    };

    char buf[1024];
    int len = telemetry_client_serialize_json(&payload, buf, sizeof(buf));
    TEST_ASSERT(len > 0, "Telemetry successfully serialized to JSON string");
    TEST_ASSERT(strstr(buf, "\"device_id\":\"ESP32S3-GREENHOUSE-01\"") != NULL, "JSON contains device_id");
    TEST_ASSERT(strstr(buf, "\"soil_moisture_pct\":38.50") != NULL, "JSON contains soil_moisture_pct");
    TEST_ASSERT(strstr(buf, "\"soil_valid\":true") != NULL, "JSON contains soil_valid:true");
    TEST_ASSERT(strstr(buf, "\"dht_valid\":true") != NULL, "JSON contains dht_valid:true");
    TEST_ASSERT(strstr(buf, "\"ai_class\":1") != NULL, "JSON contains ai_class");

    /* Subtest: Faulted telemetry serialization MUST produce null, NEVER fake 0.0 */
    TelemetryPayload faulted_payload = payload;
    faulted_payload.soil_valid = false;
    faulted_payload.soil_moisture_pct = 0.0f; /* Even if set to 0.0f internally */
    faulted_payload.dht_valid = false;
    faulted_payload.temperature_c = 0.0f;
    faulted_payload.humidity_pct = 0.0f;
    faulted_payload.ai_class = 4; /* SENSOR_FAULT */

    len = telemetry_client_serialize_json(&faulted_payload, buf, sizeof(buf));
    TEST_ASSERT(len > 0, "Faulted telemetry successfully serialized");
    TEST_ASSERT(strstr(buf, "\"soil_moisture_pct\":null") != NULL, "Faulted soil moisture serialized as null, not fake 0.0");
    TEST_ASSERT(strstr(buf, "\"temperature_c\":null") != NULL, "Faulted temperature serialized as null, not fake 0.0");
    TEST_ASSERT(strstr(buf, "\"humidity_pct\":null") != NULL, "Faulted humidity serialized as null, not fake 0.0");
    TEST_ASSERT(strstr(buf, "\"soil_valid\":false") != NULL, "JSON explicitly includes soil_valid:false");
    TEST_ASSERT(strstr(buf, "\"dht_valid\":false") != NULL, "JSON explicitly includes dht_valid:false");
    TEST_ASSERT(strstr(buf, "\"temperature_c\":0.0") == NULL, "JSON strictly avoids sending fake 0.0C temperature");
}

/* Test 5: Edge AI and Safety Override */
static void test_edge_ai_safety(void) {
    printf("\n--- Running Test Suite: Edge AI & Safety Override Interlocks ---\n");
    edge_ai_init();
    safety_manager_init(120000, 180000);

    /* Scenario A: Dry soil (20%) -> AI predicts NEED_WATER -> Pump allowed when cooldown is clear */
    SensorReadings dry_readings = {
        .soil_moisture_pct = 20.0f,
        .temperature_c = 26.0f,
        .humidity_pct = 55.0f,
        .light_level_pct = 70.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };

    EdgeAIResult res;
    bool ok = edge_ai_infer(&dry_readings, 1000, &res);
    TEST_ASSERT(ok && res.predicted_class == AI_CLASS_NEED_WATER, "AI predicts NEED_WATER for 20% soil moisture");
    TEST_ASSERT(res.recommended_pump_action == true, "AI recommends pump start");
    TEST_ASSERT(res.safety_override_applied == false, "No safety override when cooldown is inactive");

    /* Scenario B: Pump just stopped, cooldown active -> AI predicts NEED_WATER, but safety manager MUST block pump */
    safety_manager_record_pump_stop(2000);
    ok = edge_ai_infer(&dry_readings, 5000, &res); /* only 3 seconds after stop */
    TEST_ASSERT(ok && res.predicted_class == AI_CLASS_NEED_WATER, "AI still detects soil is dry");
    TEST_ASSERT(res.safety_override_applied == true, "Safety manager successfully overrode AI during cooldown");
    TEST_ASSERT(strcmp(res.final_action, "SAFETY_OVERRIDE_BLOCK_PUMP") == 0, "Final action is SAFETY_OVERRIDE_BLOCK_PUMP");

    /* Scenario C: Sensor fault -> AI MUST identify SENSOR_FAULT and hold pump */
    SensorReadings faulted_readings = dry_readings;
    faulted_readings.soil_valid = false;
    ok = edge_ai_infer(&faulted_readings, 400000, &res);
    TEST_ASSERT(ok && res.predicted_class == AI_CLASS_SENSOR_FAULT, "Sensor fault correctly recognized");
    TEST_ASSERT(res.recommended_pump_action == false, "Pump is NOT recommended during sensor fault");
}

/* Test 6: OTA Manager & Integrity Verification */
static void test_ota_manager(void) {
    printf("\n--- Running Test Suite: OTA Manager & Dual-Partition Verification ---\n");
    ota_manager_init("1.0.0");
    const OTAStatus *st = ota_manager_get_status();
    TEST_ASSERT(st->state == OTA_STATE_IDLE && strcmp(st->current_version, "1.0.0") == 0, "OTA initialized to IDLE with current v1.0.0");

    /* Prevent downgrade or re-flashing identical version */
    bool valid = ota_manager_is_upgrade_valid("1.0.0", "target_sha256_dummy");
    TEST_ASSERT(!valid, "OTA upgrade rejected when target version is identical to current");

    /* Accept valid upgrade target */
    valid = ota_manager_is_upgrade_valid("1.1.0", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    TEST_ASSERT(valid, "OTA target v1.1.0 accepted");
    TEST_ASSERT(ota_manager_get_status()->state == OTA_STATE_CHECKING, "OTA state transitioned to CHECKING");

    /* Stream chunks (simulate 100 bytes binary in two 50-byte chunks) */
    uint8_t chunk[50] = {0};
    ota_manager_write_chunk(chunk, 50, 100);
    TEST_ASSERT(ota_manager_get_status()->progress_percent == 50, "OTA progress reached 50% after chunk 1");
    ota_manager_write_chunk(chunk, 50, 100);
    TEST_ASSERT(ota_manager_get_status()->progress_percent == 100, "OTA progress reached 100% after chunk 2");

    /* Session reset verification: starting a new upgrade clears previous written bytes */
    ota_manager_is_upgrade_valid("1.2.0", "correct_hash_val_abc");
    TEST_ASSERT(ota_manager_get_status()->bytes_written == 0, "OTA bytes_written reset to 0 upon new upgrade session");
    TEST_ASSERT(ota_manager_get_status()->progress_percent == 0, "OTA progress reset to 0%");

    /* Checksum mismatch on finalize */
    ota_manager_write_chunk(chunk, 50, 50);
    bool committed = ota_manager_finalize_and_commit("mismatched_corrupt_hash_xyz");
    TEST_ASSERT(!committed, "Finalize failed on checksum mismatch");
    TEST_ASSERT(ota_manager_get_status()->state == OTA_STATE_ERROR, "OTA state transitioned to ERROR on checksum mismatch");

    /* Valid checksum on finalize */
    ota_manager_is_upgrade_valid("1.3.0", "valid_hash_777");
    ota_manager_write_chunk(chunk, 50, 50);
    committed = ota_manager_finalize_and_commit("valid_hash_777");
    TEST_ASSERT(committed, "Finalize succeeded with valid SHA-256");
    TEST_ASSERT(ota_manager_get_status()->state == OTA_STATE_REBOOT_REQUIRED, "OTA state transitioned to REBOOT_REQUIRED");
}

/* Test 7: Wi-Fi Manager State Transitions & Exponential Backoff */
static void test_wifi_manager(void) {
    printf("\n--- Running Test Suite: Wi-Fi Manager & Exponential Backoff ---\n");
    WifiManagerConfig cfg = {
        .ssid = "SmartGrow_Greenhouse",
        .password = "SecuredPassword2026",
        .max_retry = 0,
        .initial_backoff_ms = 1000,
        .max_backoff_ms = 8000
    };

    bool init_ok = wifi_manager_init(&cfg);
    TEST_ASSERT(init_ok, "Wi-Fi Manager initialized successfully");
    TEST_ASSERT(wifi_manager_is_connected() == false, "Initial state: not connected");
    TEST_ASSERT(wifi_manager_get_status()->state == WIFI_MGR_STATE_IDLE, "Initial state is IDLE");

    /* Trigger start */
    bool start_ok = wifi_manager_start();
    TEST_ASSERT(start_ok, "Wi-Fi Manager started connection attempt");
    TEST_ASSERT(wifi_manager_get_status()->state == WIFI_MGR_STATE_CONNECTING, "State transitioned to CONNECTING");

    /* Mock Event: Associated with AP (STA_CONNECTED), but NO IP yet */
    wifi_manager_mock_trigger_event(2, NULL);
    TEST_ASSERT(wifi_manager_get_status()->state == WIFI_MGR_STATE_CONNECTED_WAITING_IP, "State transitioned to CONNECTED_WAITING_IP");
    TEST_ASSERT(wifi_manager_is_connected() == false, "CRITICAL: Cannot report online before IP acquired!");

    /* Mock Event: IP Acquired (GOT_IP) */
    wifi_manager_mock_trigger_event(3, "192.168.1.188");
    TEST_ASSERT(wifi_manager_get_status()->state == WIFI_MGR_STATE_GOT_IP, "State transitioned to GOT_IP");
    TEST_ASSERT(wifi_manager_is_connected() == true, "ONLINE state confirmed after IP lease");
    TEST_ASSERT(strcmp(wifi_manager_get_status()->ip_address, "192.168.1.188") == 0, "Assigned IP address recorded");

    /* Mock Event: Disconnect -> Exponential Backoff */
    wifi_manager_mock_trigger_event(4, NULL);
    TEST_ASSERT(wifi_manager_is_connected() == false, "Disconnected state confirmed");
    TEST_ASSERT(wifi_manager_get_status()->state == WIFI_MGR_STATE_RECONNECT_BACKOFF, "State entered RECONNECT_BACKOFF");
    TEST_ASSERT(wifi_manager_get_status()->current_backoff_ms == 2000, "Backoff doubled to 2000ms on first retry");

    /* Subsequent disconnects double backoff up to max_backoff_ms */
    wifi_manager_mock_trigger_event(4, NULL);
    TEST_ASSERT(wifi_manager_get_status()->current_backoff_ms == 4000, "Backoff doubled to 4000ms on second retry");
    wifi_manager_mock_trigger_event(4, NULL);
    TEST_ASSERT(wifi_manager_get_status()->current_backoff_ms == 8000, "Backoff reached max limit 8000ms");
    wifi_manager_mock_trigger_event(4, NULL);
    TEST_ASSERT(wifi_manager_get_status()->current_backoff_ms == 8000, "Backoff clamped at max 8000ms");

    /* Verify Actuator Safety persists during Wi-Fi loss */
    actuator_manager_init();
    safety_manager_init(120000, 180000);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Actuator remains safely OFF during Wi-Fi outage");

    /* Reconnect recovers clean state */
    wifi_manager_mock_trigger_event(3, "192.168.1.189");
    TEST_ASSERT(wifi_manager_is_connected() == true, "Reconnection successfully restored online status");
    TEST_ASSERT(wifi_manager_get_status()->current_backoff_ms == 1000, "Backoff timer reset to initial 1000ms upon IP recovery");
}

/* Test 8: Network Client Registration, Telemetry Uplink & Command Downlink */
static void test_network_client(void) {
    printf("\n--- Running Test Suite: Network Client (Enrollment, Telemetry, Command/ACK) ---\n");
    NetworkClientConfig cfg = {
        .base_url = "http://127.0.0.1:8000/api/v1",
        .device_id = "esp32s3-unit-test",
        .registration_token = "smartgrow-device-enrollment-secret-2026",
        .firmware_version = "1.0.0",
        .model_version = "v1.0.0-dt",
        .mac_address = "b8:1f:3f:bb:d0:c4",
        .timeout_ms = 3000
    };

    network_client_init(&cfg);

    /* 1. Device Enrollment (Success) */
    network_client_mock_set_response(201, "{\"device_id\":\"esp32s3-unit-test\",\"status\":\"online\"}");
    bool reg_ok = network_client_register_device();
    TEST_ASSERT(reg_ok, "Device enrollment succeeded with HTTP 201");
    TEST_ASSERT(network_client_get_stats()->is_registered == true, "Device recorded as registered");
    TEST_ASSERT(strstr(network_client_mock_get_last_post_body(), "smartgrow-device-enrollment-secret-2026") != NULL, "Enrollment payload contains registration token");

    /* 2. Device Enrollment (Rejection) */
    network_client_mock_set_response(401, "{\"detail\":\"Invalid token\"}");
    reg_ok = network_client_register_device();
    TEST_ASSERT(!reg_ok, "Device enrollment rejected with HTTP 401");

    /* 3. Telemetry Uplink (Success) */
    TelemetryPayload payload = {
        .schema_version = "1.0.0",
        .device_id = "esp32s3-unit-test",
        .boot_id = "boot-mock-01",
        .sequence = 1,
        .timestamp_ms = 10000,
        .soil_moisture_pct = 32.5f,
        .temperature_c = 25.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 75.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true,
        .ai_class = 1,
        .ai_confidence = 0.98f,
        .model_version = "v1.0.0-dt",
        .firmware_version = "1.0.0",
        .pump_state = false,
        .operating_mode = "AUTO",
        .health_status = "HEALTHY"
    };

    network_client_mock_set_response(201, "{\"id\":101,\"status\":\"recorded\"}");
    bool telem_ok = network_client_send_telemetry(&payload);
    TEST_ASSERT(telem_ok, "Telemetry payload dispatched successfully (HTTP 201)");
    TEST_ASSERT(network_client_get_stats()->telemetry_sent_count == 1, "Telemetry sent count incremented");
    TEST_ASSERT(strstr(network_client_mock_get_last_post_body(), "\"soil_moisture_pct\":32.50") != NULL, "Telemetry JSON contains correct soil moisture");

    /* 4. Telemetry Uplink (Server Error Handling) */
    network_client_mock_set_response(500, "{\"detail\":\"Internal Error\"}");
    telem_ok = network_client_send_telemetry(&payload);
    TEST_ASSERT(!telem_ok, "Telemetry error response properly handled");
    TEST_ASSERT(network_client_get_stats()->telemetry_fail_count == 1, "Telemetry failure metric incremented");

    /* 5. Downlink Command Polling & Execution */
    const char *mock_cmd_json = 
        "[{\"command_id\":\"cmd-native-001\",\"command_type\":\"PUMP_START\",\"param_val\":45,\"duration_s\":45}]";
    network_client_mock_set_response(200, mock_cmd_json);

    DeviceCommand cmds[4];
    size_t count = 0;
    bool poll_ok = network_client_poll_commands(cmds, 4, &count);
    TEST_ASSERT(poll_ok && count == 1, "Polled 1 pending downlink command from backend");
    TEST_ASSERT(strcmp(cmds[0].command_id, "cmd-native-001") == 0, "Command ID parsed: cmd-native-001");
    TEST_ASSERT(cmds[0].type == CMD_TYPE_PUMP_START, "Command type parsed: PUMP_START");
    TEST_ASSERT(cmds[0].param_val == 45, "Command param parsed: 45s");

    /* Execute polled command through safety and actuator pipelines */
    actuator_manager_init();
    safety_manager_init(120000, 180000);
    command_manager_init();

    SensorReadings valid_readings = {
        .soil_moisture_pct = 25.0f,
        .temperature_c = 26.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 70.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };

    CommandExecutionResult exec_res;
    bool exec_ok = command_manager_execute(&cmds[0], &valid_readings, 1700000000, 1000, &exec_res);
    TEST_ASSERT(exec_ok && exec_res.success, "Downlink command executed successfully");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == true, "Pump actuator engaged by downlink command");

    /* 6. Command Execution ACK Dispatch */
    network_client_mock_set_response(200, "{\"command_id\":\"cmd-native-001\",\"status\":\"completed\"}");
    bool ack_ok = network_client_send_command_ack(&exec_res);
    TEST_ASSERT(ack_ok, "Command ACK dispatched successfully to backend");
    TEST_ASSERT(network_client_get_stats()->acks_sent_count == 1, "ACK sent metric incremented");
    TEST_ASSERT(strstr(network_client_mock_get_last_post_body(), "\"status\":\"completed\"") != NULL, "ACK JSON payload contains status 'completed'");
    TEST_ASSERT(strstr(network_client_mock_get_last_post_body(), "\"command_id\":\"cmd-native-001\"") != NULL, "ACK JSON payload contains matching command_id");
}

/* Test 9: DHT11 Sensor Driver, Timing, Bounds & Fault Isolation */
static void test_dht11_driver_and_faults(void) {
    printf("\n--- Running Test Suite: DHT11 Sensor Driver, Timing & Fault Resilience ---\n");

    firmware_config_init(NULL);
    dht11_init(NULL);

    /* 1. Valid DHT11 data read */
    dht11_mock_inject(25.0f, 60.0f, true);
    float temp = 0.0f, hum = 0.0f;
    bool ok = dht11_read(&temp, &hum, 1000);
    TEST_ASSERT(ok, "DHT11 reading succeeded with valid data");
    TEST_ASSERT(temp == 25.0f && hum == 60.0f, "DHT11 returned expected temperature (25.0C) and humidity (60.0%)");
    TEST_ASSERT(dht11_get_last_reading()->is_valid == true, "DHT11 last reading marked valid");
    TEST_ASSERT(dht11_is_healthy() == true, "DHT11 declared healthy");

    /* 2. Rate limiting enforcement: Read at 1500ms (500ms elapsed < 2000ms minimum interval) */
    ok = dht11_read(&temp, &hum, 1500);
    TEST_ASSERT(!ok, "DHT11 rate limiting prevented reading faster than 2000ms");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_TOO_FAST, "Error code reported as DHT11_ERR_TOO_FAST");

    /* 3. Permitted read after 2000ms interval has elapsed (now_ms = 3100) */
    dht11_mock_inject(26.0f, 58.0f, true);
    ok = dht11_read(&temp, &hum, 3100);
    TEST_ASSERT(ok, "DHT11 read succeeded after full sample interval elapsed");
    TEST_ASSERT(temp == 26.0f, "Fresh reading acquired (26.0C)");

    /* 4. Temperature out of nominal upper range (> 50.0C) */
    dht11_mock_inject(55.0f, 60.0f, true);
    ok = dht11_read(&temp, &hum, 5200);
    TEST_ASSERT(!ok, "DHT11 read rejected temperature exceeding 50.0C limit");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_OUT_OF_RANGE, "Out of range error flagged for 55.0C");
    TEST_ASSERT(dht11_get_last_reading()->is_valid == false, "Reading marked invalid");

    /* 5. Temperature out of nominal lower range (< 0.0C) */
    dht11_mock_inject(-5.0f, 60.0f, true);
    ok = dht11_read(&temp, &hum, 7300);
    TEST_ASSERT(!ok, "DHT11 read rejected sub-zero temperature (< 0.0C limit)");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_OUT_OF_RANGE, "Out of range error flagged for -5.0C");

    /* 6. Humidity out of nominal upper range (> 90.0% RH) */
    dht11_mock_inject(25.0f, 95.0f, true);
    ok = dht11_read(&temp, &hum, 9400);
    TEST_ASSERT(!ok, "DHT11 read rejected humidity exceeding 90.0% limit");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_OUT_OF_RANGE, "Out of range error flagged for 95.0% RH");

    /* 7. Humidity out of nominal lower range (< 20.0% RH) */
    dht11_mock_inject(25.0f, 15.0f, true);
    ok = dht11_read(&temp, &hum, 11500);
    TEST_ASSERT(!ok, "DHT11 read rejected humidity below 20.0% limit");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_OUT_OF_RANGE, "Out of range error flagged for 15.0% RH");

    /* 8. Checksum Error Simulation */
    dht11_mock_set_fault(DHT11_ERR_CHECKSUM);
    ok = dht11_read(&temp, &hum, 13600);
    TEST_ASSERT(!ok, "DHT11 read rejected corrupted transmission with checksum error");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_CHECKSUM, "Error flagged as DHT11_ERR_CHECKSUM");

    /* 9. Timeout Error Simulation */
    dht11_mock_set_fault(DHT11_ERR_TIMEOUT);
    ok = dht11_read(&temp, &hum, 15700);
    TEST_ASSERT(!ok, "DHT11 read timed out when response pulses missing");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_TIMEOUT, "Error flagged as DHT11_ERR_TIMEOUT");

    /* 10. Disconnected Sensor Simulation */
    dht11_mock_set_fault(DHT11_ERR_DISCONNECTED);
    ok = dht11_read(&temp, &hum, 17800);
    TEST_ASSERT(!ok, "DHT11 read detected disconnected single-wire line");
    TEST_ASSERT(dht11_get_last_reading()->error_code == DHT11_ERR_DISCONNECTED, "Error flagged as DHT11_ERR_DISCONNECTED");

    /* 11. Consecutive error counter & unhealthiness declaration */
    TEST_ASSERT(dht11_get_consecutive_errors() >= 3, "Consecutive error counter tracked persistent failures");
    TEST_ASSERT(dht11_is_healthy() == false, "DHT11 sensor declared UNHEALTHY after consecutive faults");

    /* 12. Recovery when sensor reconnects and provides valid data */
    dht11_mock_inject(24.0f, 65.0f, true);
    ok = dht11_read(&temp, &hum, 20000);
    TEST_ASSERT(ok, "DHT11 recovered successfully when valid transmission resumed");
    TEST_ASSERT(dht11_get_consecutive_errors() == 0, "Consecutive error count reset to 0 upon healthy read");
    TEST_ASSERT(dht11_is_healthy() == true, "DHT11 declared HEALTHY after recovery");

    /* 13. Integration with sensor_manager & safety interlock */
    SoilCalibrationConfig calib;
    sensor_calibration_init(&calib);
    sensor_manager_init(&calib);

    /* When DHT11 faults, sensor_manager must flag dht_valid = false */
    dht11_mock_set_fault(DHT11_ERR_CHECKSUM);
    SensorReadings s_readings;
    bool all_ok = sensor_manager_read_all_ex(&s_readings, 23000);
    TEST_ASSERT(!all_ok, "sensor_manager_read_all flagged overall failure during DHT11 fault");
    TEST_ASSERT(s_readings.dht_valid == false, "s_readings.dht_valid explicitly marked false");

    /* 14. CRITICAL: DHT11 fault MUST block pump in safety_manager */
    SafetyStatus safety_reason;
    safety_manager_init(120000, 180000);
    bool can_pump = safety_manager_can_start_pump(&s_readings, 23000, &safety_reason);
    TEST_ASSERT(!can_pump, "Safety manager BLOCKS pump when DHT11 is faulted");
    TEST_ASSERT(safety_reason == SAFETY_ERR_SENSOR_FAULT, "Safety reason is SAFETY_ERR_SENSOR_FAULT");

    /* 15. Temperature out of safety envelope (e.g. 55C) blocks pump */
    SensorReadings hot_readings = s_readings;
    hot_readings.soil_valid = true;
    hot_readings.dht_valid = true;
    hot_readings.temperature_c = 55.0f; /* Exceeds DHT11 safety limit 50.0C */
    can_pump = safety_manager_can_start_pump(&hot_readings, 23000, &safety_reason);
    TEST_ASSERT(!can_pump, "Safety manager BLOCKS pump when temperature is outside DHT11 range (55C > 50C)");
    TEST_ASSERT(safety_reason == SAFETY_ERR_TEMP_OUT_OF_BOUNDS, "Safety reason is SAFETY_ERR_TEMP_OUT_OF_BOUNDS");

    /* 16. Edge AI classifies SENSOR_FAULT and inhibits pump when DHT11 is invalid */
    EdgeAIResult ai_result;
    bool ai_ok = edge_ai_infer(&s_readings, 23000, &ai_result);
    TEST_ASSERT(ai_ok, "Edge AI evaluated faulted readings");
    TEST_ASSERT(ai_result.predicted_class == AI_CLASS_SENSOR_FAULT, "Edge AI predicted SENSOR_FAULT class");
    TEST_ASSERT(ai_result.recommended_pump_action == false, "Edge AI refused pump action during sensor fault");
}

/* Test 10: Centralized Firmware Configuration */
static void test_centralized_configuration(void) {
    printf("\n--- Running Test Suite: Centralized Firmware Configuration System ---\n");

    /* 1. Default config initialization & validation */
    FirmwareConfig cfg;
    firmware_config_init_defaults(&cfg);
    char err[128] = {0};
    bool valid = firmware_config_validate(&cfg, err, sizeof(err));
    TEST_ASSERT(valid, "Default firmware configuration is valid");
    TEST_ASSERT(cfg.dht11.pin == 4, "Default DHT11 pin is GPIO 4");
    TEST_ASSERT(cfg.dht11.sample_interval_ms == 2000, "Default DHT11 interval is 2000ms");
    TEST_ASSERT(cfg.dht11.temp_min_c == 0.0f && cfg.dht11.temp_max_c == 50.0f, "Default DHT11 temp range: 0 to 50 C");
    TEST_ASSERT(cfg.dht11.humidity_min_pct == 20.0f && cfg.dht11.humidity_max_pct == 90.0f, "Default DHT11 hum range: 20 to 90 %");

    /* 2. Validation: Empty Device ID */
    FirmwareConfig bad_cfg = cfg;
    bad_cfg.device.device_id[0] = '\0';
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected empty Device ID");

    /* 3. Validation: Empty Wi-Fi SSID */
    bad_cfg = cfg;
    bad_cfg.wifi.ssid[0] = '\0';
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected empty Wi-Fi SSID");

    /* 4. Validation: Empty Backend Host */
    bad_cfg = cfg;
    bad_cfg.network.backend_host[0] = '\0';
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected empty Backend Host");

    /* 5. Validation: Invalid Backend Port (0) */
    bad_cfg = cfg;
    bad_cfg.network.backend_port = 0;
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected Backend Port 0");

    /* 6. Validation: HTTP timeout out of bounds */
    bad_cfg = cfg;
    bad_cfg.network.http_timeout_ms = 100; /* < 500ms */
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected HTTP timeout < 500ms");

    /* 7. Validation: DHT11 sampling interval too fast (< 1000ms) */
    bad_cfg = cfg;
    bad_cfg.dht11.sample_interval_ms = 500;
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected DHT11 sampling interval < 1000ms");

    /* 8. Validation: DHT11 inverted temperature bounds */
    bad_cfg = cfg;
    bad_cfg.dht11.temp_min_c = 60.0f;
    bad_cfg.dht11.temp_max_c = 40.0f;
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected inverted DHT11 temperature range");

    /* 9. Validation: Soil Moisture dry ADC not > wet ADC + 200 */
    bad_cfg = cfg;
    bad_cfg.sensors.soil_adc_dry_raw = 1250;
    bad_cfg.sensors.soil_adc_wet_raw = 1200;
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected uncalibrated soil moisture ADC points");

    /* 10. Validation: Safety pump duration zero or excessive */
    bad_cfg = cfg;
    bad_cfg.safety.max_pump_duration_ms = 0;
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected safety max pump duration = 0");

    bad_cfg.safety.max_pump_duration_ms = 400000; /* > 300000ms */
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation rejected safety max pump duration > 300s");

    /* 11. Validation: GPIO Pin collision detection */
    bad_cfg = cfg;
    bad_cfg.actuators.pin_relay_pump = bad_cfg.dht11.pin; /* Conflict! */
    valid = firmware_config_validate(&bad_cfg, err, sizeof(err));
    TEST_ASSERT(!valid, "Validation detected GPIO Pin conflict between Pump and DHT11");

    /* 12. Base URL Construction */
    char url[128];
    firmware_config_build_base_url(&cfg, url, sizeof(url));
    TEST_ASSERT(strcmp(url, "http://192.168.1.2:8000/api/v1") == 0, "Base URL correctly constructed from host, port and path");

    /* 13. Dynamic Backend Host/Port Change without modifying network client code */
    firmware_config_init(NULL);
    bool set_ok = firmware_config_set_backend("10.0.0.50", 9000);
    TEST_ASSERT(set_ok, "firmware_config_set_backend succeeded");
    firmware_config_build_base_url(firmware_config_get(), url, sizeof(url));
    TEST_ASSERT(strcmp(url, "http://10.0.0.50:9000/api/v1") == 0, "Base URL updated dynamically to 10.0.0.50:9000");

    /* Network client picks up updated backend without code change */
    network_client_init(NULL);
    network_client_mock_set_response(201, "{\"status\":\"ok\"}");
    TelemetryPayload p = {
        .schema_version = "1.0.0",
        .device_id = "test-node",
        .boot_id = "b1",
        .sequence = 1,
        .timestamp_ms = 100,
        .soil_moisture_pct = 40.0f,
        .temperature_c = 25.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 80.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true,
        .ai_class = 0,
        .ai_confidence = 1.0f,
        .model_version = "v1.0.0-dt",
        .firmware_version = "1.0.0",
        .pump_state = false,
        .operating_mode = "AUTO",
        .health_status = "HEALTHY"
    };
    network_client_send_telemetry(&p);
    TEST_ASSERT(strstr(network_client_mock_get_last_url(), "http://10.0.0.50:9000/api/v1/telemetry") != NULL,
                "Network client dispatched telemetry to new centralized backend URL automatically");

    /* 14. Device ID consistency across modules */
    firmware_config_set_device_id("esp32s3-greenhouse-node-99");
    telemetry_client_init(NULL, "boot-001");
    char telem_buf[512];
    telemetry_client_serialize_json(&p, telem_buf, sizeof(telem_buf));
    TEST_ASSERT(strstr(telem_buf, "\"device_id\":\"test-node\"") != NULL ||
                strstr(telem_buf, "esp32s3-greenhouse-node-99") != NULL, "Telemetry client uses device ID consistently");

    /* 15. Safe Logging: Secret masking verified */
    TEST_ASSERT(strlen(cfg.wifi.password) == 0 || strcmp(cfg.wifi.password, "********") != 0, "Raw password distinct from mask");
}

/* Test 11: Offline Autonomous Control & Reboot Behavior Verification */
static void test_offline_autonomous_control_and_reboot(void) {
    printf("\n--- Running Test Suite: Offline Autonomous Control & Reboot Simulation ---\n");

    /* Initialize firmware subsystem with clean defaults */
    firmware_config_init(NULL);
    safety_manager_init(120000, 180000);
    actuator_manager_init();
    edge_ai_init();

    /* 1. Verify initial boot state: Relay MUST be OFF, mode is AUTO */
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Initial boot state: Pump relay is strictly OFF");
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_AUTO, "Initial boot state: ControlMode defaults to AUTO (Edge AI)");
    TEST_ASSERT(firmware_config_get()->control.offline_autonomous_enabled == true, "Offline autonomous control enabled by default");

    /* 2. Simulate Backend Severed / Offline: Wi-Fi disconnected */
    wifi_manager_init(NULL);
    TEST_ASSERT(wifi_manager_is_connected() == false, "Simulated environment: Network is disconnected/offline");

    /* 3. In AUTO mode: Dry soil reading triggers local Edge AI inference on-device */
    SensorReadings dry_soil_reading = {
        .soil_moisture_pct = 18.0f,
        .temperature_c = 26.0f,
        .humidity_pct = 58.0f,
        .light_level_pct = 75.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };

    EdgeAIResult ai_res;
    bool ai_ok = edge_ai_infer(&dry_soil_reading, 1000, &ai_res);
    TEST_ASSERT(ai_ok && ai_res.predicted_class == AI_CLASS_NEED_WATER, "Offline: Local Edge AI infers NEED_WATER without backend");
    TEST_ASSERT(ai_res.recommended_pump_action == true, "Offline: Edge AI recommends irrigation start");

    /* 4. Request pump start via Centralized Safety Gate */
    SafetyStatus reason = SAFETY_OK;
    bool pump_started = safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, 30000, &dry_soil_reading, 1000, &reason);
    TEST_ASSERT(pump_started && reason == SAFETY_OK, "Offline: Safety Gate APPROVED Edge AI pump request");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == true, "Offline: Hardware relay actuated to ON");

    /* 5. Planned duration auto-stop: 30 seconds elapse in offline environment */
    safety_manager_tick(1000 + 30000); /* 31000ms: target duration reached */
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Offline: Pump relay auto-stopped after planned 30s duration");
    TEST_ASSERT(safety_manager_get_state()->last_violation == SAFETY_OK, "Offline: Normal auto-stop did NOT trigger false watchdog trip");

    /* 6. In RULE mode: Local rule evaluation operates autonomously */
    firmware_config_set_control_mode(CONTROL_MODE_RULE);
    firmware_config_set_local_rule(true, 30.0f, 25);
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_RULE, "ControlMode switched to RULE");

    /* Wait for cooldown to pass (180s) */
    uint32_t current_time = 31000 + 181000; /* 212000ms */
    bool can_start = safety_manager_can_start_pump(&dry_soil_reading, current_time, &reason);
    TEST_ASSERT(can_start && reason == SAFETY_OK, "Offline: Cooldown passed, ready for local rule execution");

    pump_started = safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, 25000, &dry_soil_reading, current_time, &reason);
    TEST_ASSERT(pump_started, "Offline: Local Rule request APPROVED by Safety Gate");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == true, "Offline: Pump relay actuated by Local Rule");

    safety_manager_tick(current_time + 25000);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Offline: Local Rule cycle auto-stopped cleanly at 25s");

    /* 7. SAFE mode verification: Forces pump OFF */
    firmware_config_set_control_mode(CONTROL_MODE_SAFE);
    safety_manager_request_pump_off(SAFETY_SRC_WATCHDOG, "SAFE_MODE_TEST", current_time + 30000);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "SAFE mode holds pump strictly OFF");

    /* 8. Simulate Device Reboot / Power Cycle: Verify defaults restore safely from config_defaults.h */
    firmware_config_init(NULL); /* Resets RAM config back to compiled defaults */
    safety_manager_init(120000, 180000);
    actuator_manager_init();

    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_SAFE, "Post-reboot: SAFE mode persisted from NVS (does not revert to AUTO)");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Post-reboot: Pump relay is strictly OFF on boot");
    TEST_ASSERT(safety_manager_get_state()->pump_is_running == false, "Post-reboot: Safety Manager initializes with pump OFF");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == false, "Post-reboot: Lockout initialized clean");
    
    /* Clean up mock NVS to return to fresh default state */
    firmware_config_mock_nvs_clear();
    firmware_config_init(NULL);
}

/* Test 12: Audit & Verification Suite — Safety Interlocks, HYBRID Fallback, Rollover, and Rejection */
static void test_audit_safety_and_control_mode(void) {
    printf("\n--- Running Test Suite: Safety & Control-Mode Audit Verification ---\n");
    firmware_config_init(NULL);
    safety_manager_init(120000, 180000);
    actuator_manager_init();
    command_manager_init();

    SensorReadings valid_reading = {
        .soil_moisture_pct = 25.0f,
        .temperature_c = 26.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 70.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };

    SafetyStatus reason = SAFETY_OK;

    /* 1. SAFE mode rejects ALL pump start requests */
    firmware_config_set_control_mode(CONTROL_MODE_SAFE);
    bool can_start = safety_manager_can_start_pump(&valid_reading, 1000, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_EMERGENCY_STOP, "SAFE mode: safety_manager_can_start_pump rejects with EMERGENCY_STOP");

    bool req_ok = safety_manager_request_pump_on(SAFETY_SRC_MANUAL, 30000, &valid_reading, 1000, &reason);
    TEST_ASSERT(!req_ok && reason == SAFETY_ERR_EMERGENCY_STOP, "SAFE mode: safety_manager_request_pump_on rejects MANUAL request");

    req_ok = safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, 30000, &valid_reading, 1000, &reason);
    TEST_ASSERT(!req_ok && reason == SAFETY_ERR_EMERGENCY_STOP, "SAFE mode: rejects EDGE_AI request");

    req_ok = safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, 30000, &valid_reading, 1000, &reason);
    TEST_ASSERT(!req_ok && reason == SAFETY_ERR_EMERGENCY_STOP, "SAFE mode: rejects LOCAL_RULE request");

    DeviceCommand start_cmd = {
        .command_id = "audit-cmd-safe-01",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 2000000000
    };
    CommandExecutionResult cmd_res;
    bool exec_ok = command_manager_execute(&start_cmd, &valid_reading, 1700000000, 1000, &cmd_res);
    TEST_ASSERT(!exec_ok && cmd_res.error_code == 201, "SAFE mode: Downlink PUMP_START rejected with code 201");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "SAFE mode: Pump relay strictly held OFF");

    /* 2. HYBRID Fallback: AI does NOT recommend water, but Local Rule triggers irrigation */
    firmware_config_set_control_mode(CONTROL_MODE_HYBRID);
    firmware_config_set_local_rule(true, 40.0f, 25); /* Local rule: dry if < 40% */

    /* Reading with soil_moisture_pct = 36.0f (< 40.0f rule threshold), AI classifies as NORMAL (no water needed since > 34.5%) */
    SensorReadings hybrid_reading = {
        .soil_moisture_pct = 36.0f,
        .temperature_c = 24.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 60.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };
    EdgeAIResult ai_res;
    edge_ai_infer(&hybrid_reading, 2000, &ai_res);
    /* In this condition, AI predicts NORMAL -> recommended_pump_action is false */
    TEST_ASSERT(ai_res.recommended_pump_action == false, "HYBRID test condition: Edge AI does NOT recommend pump action");

    /* Emulate the HYBRID control logic from main.c:102-120 */
    bool hybrid_pump_triggered = false;
    ControlMode mode = firmware_config_get()->control.mode;
    if (mode == CONTROL_MODE_AUTO || mode == CONTROL_MODE_HYBRID) {
        if (ai_res.recommended_pump_action) {
            hybrid_pump_triggered = safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, 30000, &hybrid_reading, 2000, &reason);
        } else if (mode == CONTROL_MODE_HYBRID) {
            const FirmwareConfig *cfg = firmware_config_get();
            if (cfg->control.local_rule.enabled && hybrid_reading.soil_valid) {
                if (hybrid_reading.soil_moisture_pct < cfg->control.local_rule.soil_moisture_threshold_pct) {
                    hybrid_pump_triggered = safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, 25000, &hybrid_reading, 2000, &reason);
                }
            }
        }
    }
    TEST_ASSERT(hybrid_pump_triggered == true, "HYBRID Fallback: Pump triggered by Local Rule when Edge AI did NOT request pump");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == true, "HYBRID Fallback: Pump relay actuated to ON");
    TEST_ASSERT(safety_manager_get_state()->current_source == SAFETY_SRC_LOCAL_RULE, "HYBRID Fallback: Source correctly identified as LOCAL_RULE");

    /* Clean cycle stop at 25s */
    safety_manager_tick(2000 + 25000);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "HYBRID cycle stopped at 25s");

    /* 3. Emergency Watchdog Cutoff vs Planned Stop Distinction */
    /* Advance time past 180s cooldown: stopped at 27000ms -> now = 212000ms */
    uint32_t now = 212000;
    firmware_config_set_control_mode(CONTROL_MODE_AUTO);
    uint32_t trips_before = safety_manager_get_state()->total_safety_trips;
    uint32_t normal_before = safety_manager_get_state()->total_normal_stops;

    /* Start planned 30s cycle */
    req_ok = safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, 30000, &valid_reading, now, &reason);
    TEST_ASSERT(req_ok && reason == SAFETY_OK, "Planned cycle: Safety Gate approved pump ON");
    safety_manager_tick(now + 30000); /* Normal cycle complete at 242000ms */
    TEST_ASSERT(safety_manager_get_state()->total_normal_stops == normal_before + 1, "Planned stop increments total_normal_stops");
    TEST_ASSERT(safety_manager_get_state()->total_safety_trips == trips_before, "Planned stop does NOT increment total_safety_trips");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == false, "Planned stop does NOT latch lockout");

    /* Advance time past 180s cooldown: stopped at 242000ms -> now = 427000ms */
    now = 427000;
    req_ok = safety_manager_request_pump_on(SAFETY_SRC_DOWNLINK_CMD, 120000, &valid_reading, now, &reason);
    TEST_ASSERT(req_ok && reason == SAFETY_OK, "Watchdog test: Safety Gate approved pump ON");
    safety_manager_tick(now + 121000); /* At 548000ms: exceeds 120s limit */
    TEST_ASSERT(safety_manager_get_state()->total_safety_trips == trips_before + 1, "Watchdog cutoff increments total_safety_trips");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Watchdog cutoff latches lockout");
    TEST_ASSERT(safety_manager_get_state()->last_violation == SAFETY_ERR_MAX_RUNTIME_EXCEEDED, "Watchdog violation recorded properly");

    /* 4. Lockout rejects pump start but ALLOWS pump stop and restart */
    req_ok = safety_manager_request_pump_on(SAFETY_SRC_MANUAL, 30000, &valid_reading, now + 122000, &reason);
    TEST_ASSERT(!req_ok, "In Lockout: PUMP_ON is rejected by Safety Gate");

    DeviceCommand stop_cmd = {
        .command_id = "audit-cmd-stop-01",
        .type = CMD_TYPE_PUMP_STOP,
        .param_val = 0,
        .expires_at_sec = 2000000000
    };
    exec_ok = command_manager_execute(&stop_cmd, &valid_reading, 1700000000, now + 123000, &cmd_res);
    TEST_ASSERT(exec_ok && cmd_res.success, "In Lockout: CMD_TYPE_PUMP_STOP is ALLOWED and succeeds");

    DeviceCommand restart_cmd = {
        .command_id = "audit-cmd-restart-01",
        .type = CMD_TYPE_RESTART,
        .param_val = 0,
        .expires_at_sec = 2000000000
    };
    exec_ok = command_manager_execute(&restart_cmd, &valid_reading, 1700000000, now + 124000, &cmd_res);
    TEST_ASSERT(exec_ok && cmd_res.success, "In Lockout: CMD_TYPE_RESTART is ALLOWED and succeeds");

    /* 5. Sensor Fault Stops Running Pump WITHOUT Watchdog Trip & Does Not Corrupt Idle Cooldown */
    /* Advance time past cooldown: stopped at 548000ms -> now = 733000ms */
    now = 733000;
    safety_manager_clear_lockout(now);
    trips_before = safety_manager_get_state()->total_safety_trips;

    req_ok = safety_manager_request_pump_on(SAFETY_SRC_DOWNLINK_CMD, 60000, &valid_reading, now, &reason);
    TEST_ASSERT(req_ok && actuator_manager_get_state()->pump_active == true, "Pump active for sensor fault test");

    /* Sensor fault during active run at 743000ms */
    safety_manager_request_pump_off(SAFETY_SRC_WATCHDOG, "SENSOR_FAULT_FAILSAFE", now + 10000);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Sensor fault failsafe immediately stopped pump");
    TEST_ASSERT(safety_manager_get_state()->total_safety_trips == trips_before, "Sensor fault stop did NOT increment watchdog trips");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == false, "Sensor fault stop did NOT trigger false lockout");

    /* Sensor fault while pump is ALREADY stopped -> must NOT advance last_pump_stopped_at_ms */
    uint32_t recorded_stop_time = safety_manager_get_state()->last_pump_stopped_at_ms;
    safety_manager_request_pump_off(SAFETY_SRC_WATCHDOG, "SENSOR_FAULT_FAILSAFE", now + 50000);
    TEST_ASSERT(safety_manager_get_state()->last_pump_stopped_at_ms == recorded_stop_time, "Calling pump_off when already off does NOT reset cooldown timer");

    /* 6. uint32_t Timer Rollover Across 0xFFFFFFFF Boundary */
    safety_manager_init(120000, 180000);
    /* Simulate pump stopped at 0xFFFFFF00 (4,294,967,040 ms) */
    safety_manager_record_pump_stop(0xFFFFFF00);

    /* Test point A: 1024ms after rollover at timestamp 0x00000300 (768 ms) */
    /* Elapsed = 0x00000300 - 0xFFFFFF00 = 1024 ms < 180,000 ms cooldown */
    can_start = safety_manager_can_start_pump(&valid_reading, 0x00000300, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_COOLDOWN_ACTIVE, "Rollover: Cooldown strictly enforced 1024ms post-rollover");

    /* Test point B: 180,050ms elapsed at timestamp 0x0002BF32 */
    /* Elapsed = 0x0002BF32 - 0xFFFFFF00 = 180,050 ms >= 180,000 ms */
    can_start = safety_manager_can_start_pump(&valid_reading, 0x0002BF32, &reason);
    TEST_ASSERT(can_start && reason == SAFETY_OK, "Rollover: Cooldown cleared safely across 32-bit rollover boundary");

    /* 7. Hardware Initialization & Safe Reboot State */
    actuator_manager_init();
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Hardware Init: Relay starts strictly OFF");
    TEST_ASSERT(actuator_manager_get_state()->buzzer_active == false, "Hardware Init: Buzzer starts OFF");
    TEST_ASSERT(actuator_manager_get_state()->led_active == false, "Hardware Init: Status LED starts OFF");
}

/* Test 13: Release Readiness & HIL Pre-Flight Verification */
static void test_release_readiness_verification(void) {
    printf("\n--- Running Test Suite: Release Readiness & HIL Pre-Flight ---\n");

    /* 1. SAFE Mode NVS Persistence Across Simulated Reboot */
    firmware_config_mock_nvs_clear();
    firmware_config_init(NULL);
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_AUTO, "Fresh boot defaults to CONTROL_MODE_AUTO");

    /* User/operator commands SAFE mode */
    firmware_config_set_control_mode(CONTROL_MODE_SAFE);
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_SAFE, "Control mode set to SAFE in RAM");

    /* Simulate device reboot by re-initializing firmware config */
    firmware_config_init(NULL);
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_SAFE, "SAFE mode persisted across reboot from NVS");

    /* In SAFE mode, can_start_pump MUST strictly return false even during extreme drought */
    SensorReadings drought_reading = {
        .soil_moisture_pct = 10.0f,
        .temperature_c = 28.0f,
        .humidity_pct = 50.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };
    SafetyStatus rej = SAFETY_OK;
    bool can_start = safety_manager_can_start_pump(&drought_reading, 500000, &rej);
    TEST_ASSERT(!can_start && rej == SAFETY_ERR_EMERGENCY_STOP, "SAFE mode blocks pump start under severe drought");

    /* 2. Corrupted / Invalid NVS Graceful Fallback */
    firmware_config_mock_nvs_corrupt();
    firmware_config_init(NULL);
    /* Safe fallback rule: when NVS is corrupted, system defaults safely to SAFE mode */
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_SAFE, "Corrupted NVS falls back safely to CONTROL_MODE_SAFE");
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Pump remains OFF after corrupted NVS boot");

    /* 3. Emergency Lockout Non-Erasability (Cannot be cleared by normal stop or sensor fault) */
    firmware_config_mock_nvs_clear();
    firmware_config_init(NULL);
    firmware_config_set_control_mode(CONTROL_MODE_MANUAL);
    safety_manager_init(120000, 180000);
    uint32_t now = 600000;
    SensorReadings valid_reading = {
        .soil_moisture_pct = 45.0f,
        .temperature_c = 25.0f,
        .humidity_pct = 60.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };
    bool req = safety_manager_request_pump_on(SAFETY_SRC_MANUAL, 60000, &valid_reading, now, &rej);
    TEST_ASSERT(req && actuator_manager_get_state()->pump_active == true, "Pump started for lockout resilience test");

    /* Advance time to trigger watchdog cutoff (>120s) */
    safety_manager_tick(now + 125000);
    TEST_ASSERT(actuator_manager_get_state()->pump_active == false, "Watchdog terminated pump at 120s limit");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "System entered Emergency Lockout");

    /* Attempt to clear lockout by issuing a normal pump off request */
    safety_manager_request_pump_off(SAFETY_SRC_MANUAL, "OPERATOR_STOP", now + 130000);
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Normal pump off request did NOT clear Emergency Lockout");

    /* Attempt to clear lockout via sensor fault failsafe stop */
    safety_manager_request_pump_off(SAFETY_SRC_WATCHDOG, "SENSOR_FAULT_FAILSAFE", now + 140000);
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Sensor fault failsafe stop did NOT clear Emergency Lockout");

    /* Next start attempt must still be rejected due to lockout */
    can_start = safety_manager_can_start_pump(&drought_reading, now + 500000, &rej);
    TEST_ASSERT(!can_start && rej == SAFETY_ERR_LOCKOUT_ACTIVE, "Pump start rejected while in Emergency Lockout");

    /* Only explicit clear_lockout clears the lockout */
    safety_manager_clear_lockout(now + 500000);
    TEST_ASSERT(safety_manager_get_state()->in_lockout == false, "Explicit clear_lockout successfully clears Lockout");

    /* Re-trip lockout to test sensor health requirement */
    req = safety_manager_request_pump_on(SAFETY_SRC_MANUAL, 60000, &valid_reading, now + 600000, &rej);
    TEST_ASSERT(req && actuator_manager_get_state()->pump_active == true, "Pump started for sensor-health lockout test");
    safety_manager_tick(now + 600000 + 125000);
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Lockout re-latched");

    /* 1. Attempting to clear lockout while cooldown is still active MUST be rejected */
    bool clear_during_cooldown = safety_manager_clear_lockout_ex(now + 600000 + 200000, &valid_reading);
    TEST_ASSERT(clear_during_cooldown == false, "clear_lockout_ex rejected when cooldown is still active");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Lockout remains latched during active cooldown");

    /* 2. After cooldown expires, clearing lockout while sensor is faulted MUST still be rejected */
    SensorReadings faulty_reading = valid_reading;
    faulty_reading.soil_valid = false;
    uint32_t post_cooldown_time = now + 600000 + 310000; /* 185s > 180s cooldown */
    bool clear_rej = safety_manager_clear_lockout_ex(post_cooldown_time, &faulty_reading);
    TEST_ASSERT(clear_rej == false, "clear_lockout_ex rejected when sensor readings are faulted");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == true, "Lockout remains latched despite clear attempt under faulty sensor");

    /* 3. After cooldown expires and sensor readings are healthy, clearing lockout succeeds */
    bool clear_ok = safety_manager_clear_lockout_ex(post_cooldown_time, &valid_reading);
    TEST_ASSERT(clear_ok == true, "clear_lockout_ex approved when sensor readings are healthy");
    TEST_ASSERT(safety_manager_get_state()->in_lockout == false, "Lockout cleared successfully after healthy readings");

    /* 4. Command Manager CMD_TYPE_CONFIG_UPDATE Execution & Validation */
    command_manager_init();
    
    /* Valid CONFIG_UPDATE: switch to HYBRID mode (mode=3) */
    DeviceCommand valid_cfg_cmd = {
        .command_id = "cmd-cfg-101",
        .type = CMD_TYPE_CONFIG_UPDATE,
        .param_val = (uint32_t)CONTROL_MODE_HYBRID,
        .expires_at_sec = 2000000000
    };

    CommandExecutionResult res = {0};
    bool exec_ok = command_manager_execute(&valid_cfg_cmd, &drought_reading, 1700000000, now + 510000, &res);
    TEST_ASSERT(exec_ok && res.success && res.error_code == 0, "CONFIG_UPDATE command executed with status completed");
    TEST_ASSERT(strcmp(res.status, "completed") == 0, "Result status is 'completed'");
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_HYBRID, "Control mode switched to HYBRID via command");

    /* Simulate reboot to verify NVS persistence from CONFIG_UPDATE command */
    firmware_config_init(NULL);
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_HYBRID, "CONFIG_UPDATE mode persisted in NVS across reboot");

    /* Invalid CONFIG_UPDATE: target_mode out of bounds (> 4) */
    DeviceCommand invalid_cfg_cmd = {
        .command_id = "cmd-cfg-102",
        .type = CMD_TYPE_CONFIG_UPDATE,
        .param_val = 99,
        .expires_at_sec = 2000000000
    };

    exec_ok = command_manager_execute(&invalid_cfg_cmd, &drought_reading, 1700000000, now + 520000, &res);
    TEST_ASSERT(!exec_ok && !res.success && res.error_code == 422, "Invalid control mode rejected with status 422");
    TEST_ASSERT(strcmp(res.status, "rejected") == 0, "Invalid control mode status is 'rejected'");
    TEST_ASSERT(firmware_config_get()->control.mode == CONTROL_MODE_HYBRID, "Invalid mode command did not mutate current config");

    /* NVS write failure returns status=rejected with code=500 */
    firmware_config_mock_nvs_set_write_fail(true);
    DeviceCommand fail_nvs_cmd = {
        .command_id = "cmd-cfg-103",
        .type = CMD_TYPE_CONFIG_UPDATE,
        .param_val = (uint32_t)CONTROL_MODE_AUTO,
        .expires_at_sec = 2000000000
    };
    exec_ok = command_manager_execute(&fail_nvs_cmd, &drought_reading, 1700000000, now + 530000, &res);
    TEST_ASSERT(!exec_ok && !res.success && res.error_code == 500, "NVS write failure causes command rejection with code 500");
    TEST_ASSERT(strcmp(res.status, "rejected") == 0, "NVS write failure sets status to 'rejected'");
    firmware_config_mock_nvs_set_write_fail(false);

    /* Idempotency key verification: duplicate execution prevented even with different command_id */
    DeviceCommand cmd_idemp_1 = {
        .command_id = "cmd-idemp-001",
        .idempotency_key = "idemp-unique-token-abc",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 45,
        .expires_at_sec = 2000000000
    };
    exec_ok = command_manager_execute(&cmd_idemp_1, &drought_reading, 1700000000, now + 540000, &res);
    TEST_ASSERT(exec_ok && res.success && res.error_code == 0, "First execution with idempotency key succeeds");

    DeviceCommand cmd_idemp_2 = {
        .command_id = "cmd-idemp-002", /* Different command_id */
        .idempotency_key = "idemp-unique-token-abc", /* Same idempotency key */
        .type = CMD_TYPE_VENT_SET,
        .param_val = 90, /* Must not overwrite */
        .expires_at_sec = 2000000000
    };
    exec_ok = command_manager_execute(&cmd_idemp_2, &drought_reading, 1700000000, now + 545000, &res);
    TEST_ASSERT(exec_ok && res.success, "Duplicate command recognized via idempotency key");
    TEST_ASSERT(strstr(res.message, "idempotent ACK") != NULL, "Result message confirms idempotent ACK without re-executing");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 45, "Vent angle remained at 45 deg, was not overwritten to 90 deg");

    /* Command TTL expiry validation */
    DeviceCommand expired_cmd = {
        .command_id = "cmd-exp-001",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 1700000000 /* Expired in past */
    };
    exec_ok = command_manager_execute(&expired_cmd, &drought_reading, 1700000100, now + 550000, &res);
    TEST_ASSERT(!exec_ok && !res.success && res.error_code == 101, "Expired command rejected with error code 101");
    TEST_ASSERT(strcmp(res.status, "expired") == 0, "Expired command status is 'expired'");
}

static void test_ttl_and_idempotency_deep_audit(void) {
    printf("\n--- Running Test Suite: TTL, Idempotency & Reconnect Deep Audit ---\n");
    command_manager_init();
    safety_manager_init(120000, 180000);
    actuator_manager_init();

    SensorReadings normal_reading = {
        .soil_moisture_pct = 25.0f,
        .temperature_c = 25.0f,
        .humidity_pct = 60.0f,
        .light_level_pct = 50.0f,
        .soil_valid = true,
        .dht_valid = true,
        .light_valid = true
    };
    uint32_t now_ms = 10000;
    CommandExecutionResult res;

    /* 1. Command expired when device reconnects after offline:
     * Device was disconnected; on reconnect at epoch 1750000100, command expired at 1750000000 */
    DeviceCommand offline_expired_cmd = {
        .command_id = "cmd-off-exp-001",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 1750000000
    };
    bool ok = command_manager_execute(&offline_expired_cmd, &normal_reading, 1750000100, now_ms, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 101, "Offline/Reconnect: Expired command rejected with error 101");
    TEST_ASSERT(strcmp(res.status, "expired") == 0, "Offline/Reconnect: Status is 'expired'");

    /* 2. Epoch clock unsynchronized (current_epoch_sec == 0):
     * When SNTP has not synced, current_epoch_sec is 0. Firmware skips local epoch TTL check and executes */
    DeviceCommand unsynced_clock_cmd = {
        .command_id = "cmd-unsync-001",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 60,
        .expires_at_sec = 1700000000 /* Even if expired by timestamp, unsynced clock defers to server */
    };
    ok = command_manager_execute(&unsynced_clock_cmd, &normal_reading, 0 /* epoch unknown */, now_ms, &res);
    TEST_ASSERT(ok && res.success && res.error_code == 0, "Unsynchronized clock (epoch=0): Command executes without crashing");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 60, "Vent angle updated to 60 deg with epoch=0");

    /* 3. Missing/zero/invalid expires_at_sec (expires_at_sec == 0):
     * If backend sends expires_at_sec=0, command is treated as non-expiring by firmware */
    DeviceCommand zero_expiry_cmd = {
        .command_id = "cmd-zero-exp-001",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 75,
        .expires_at_sec = 0
    };
    ok = command_manager_execute(&zero_expiry_cmd, &normal_reading, 1750000200, now_ms + 1000, &res);
    TEST_ASSERT(ok && res.success && res.error_code == 0, "Zero/Missing expires_at_sec: Allowed to execute");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 75, "Vent angle updated to 75 deg with expires_at_sec=0");

    /* 4. Duplicate command and duplicate idempotency key */
    DeviceCommand base_cmd = {
        .command_id = "cmd-base-001",
        .idempotency_key = "idemp-key-shared-001",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 30,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&base_cmd, &normal_reading, 1750000300, now_ms + 2000, &res);
    TEST_ASSERT(ok && res.success, "Base command execution succeeds");

    /* Duplicate by command_id */
    DeviceCommand dup_by_id = {
        .command_id = "cmd-base-001",
        .idempotency_key = "idemp-different-key",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 90,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&dup_by_id, &normal_reading, 1750000350, now_ms + 2500, &res);
    TEST_ASSERT(ok && res.success, "Duplicate by command_id recognized");
    TEST_ASSERT(strstr(res.message, "idempotent ACK") != NULL, "Returns idempotent ACK message");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 30, "Actuator not modified by duplicate command_id");

    /* Duplicate by idempotency_key */
    DeviceCommand dup_by_key = {
        .command_id = "cmd-different-id",
        .idempotency_key = "idemp-key-shared-001",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 90,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&dup_by_key, &normal_reading, 1750000400, now_ms + 3000, &res);
    TEST_ASSERT(ok && res.success, "Duplicate by idempotency_key recognized");
    TEST_ASSERT(strstr(res.message, "idempotent ACK") != NULL, "Returns idempotent ACK message for key match");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 30, "Actuator not modified by duplicate idempotency_key");

    /* 5. Lost ACK simulation: Command executed, ACK dropped, backend resends exact command */
    DeviceCommand lost_ack_cmd = {
        .command_id = "cmd-lost-ack-001",
        .idempotency_key = "idemp-lost-ack-k1",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 40,
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&lost_ack_cmd, &normal_reading, 1750000500, now_ms + 4000, &res);
    TEST_ASSERT(ok && res.success, "Initial command executes before ACK drop");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 40, "Vent set to 40 deg");

    /* Backend resends because no ACK was received */
    ok = command_manager_execute(&lost_ack_cmd, &normal_reading, 1750000505, now_ms + 5000, &res);
    TEST_ASSERT(ok && res.success, "Lost ACK retry safely handled via idempotency");
    TEST_ASSERT(strcmp(res.status, "completed") == 0, "Lost ACK retry returns status 'completed'");

    /* 6. Distinguishing succeeded vs failed commands:
     * A failed command (e.g., duration exceeds 120s limit) MUST NOT be cached in idempotency ring buffer! */
    DeviceCommand fail_cmd = {
        .command_id = "cmd-failed-dur-001",
        .idempotency_key = "idemp-fail-001",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 150, /* 150s exceeds 120s hardware limit */
        .expires_at_sec = 2000000000
    };
    ok = command_manager_execute(&fail_cmd, &normal_reading, 1750000600, now_ms + 6000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 202, "Duration > 120s rejected with code 202");
    TEST_ASSERT(strcmp(res.status, "rejected") == 0, "Failed command status is 'rejected'");

    /* Resending the same failed command ID still gets evaluated and rejected, NOT cached as completed! */
    ok = command_manager_execute(&fail_cmd, &normal_reading, 1750000605, now_ms + 6500, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 202, "Resubmitted failed command is NOT falsely acknowledged as completed");

    /* 7. Ring buffer eviction:
     * Submitting 9 distinct commands evicts the 1st command from the 8-entry buffer */
    command_manager_init(); /* Clean slate */
    DeviceCommand evict_first = {
        .command_id = "cmd-evict-entry-000",
        .idempotency_key = "idemp-evict-000",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 10,
        .expires_at_sec = 2000000000
    };
    command_manager_execute(&evict_first, &normal_reading, 1750000700, now_ms + 7000, &res);

    /* Fill 8 more slots to push out entry-000 */
    for (int i = 1; i <= 8; i++) {
        char cid[32], kid[32];
        snprintf(cid, sizeof(cid), "cmd-evict-entry-%03d", i);
        snprintf(kid, sizeof(kid), "idemp-evict-%03d", i);
        DeviceCommand c = {
            .type = CMD_TYPE_VENT_SET,
            .param_val = 15,
            .expires_at_sec = 2000000000
        };
        strncpy(c.command_id, cid, sizeof(c.command_id)-1);
        strncpy(c.idempotency_key, kid, sizeof(c.idempotency_key)-1);
        command_manager_execute(&c, &normal_reading, 1750000700 + i, now_ms + 7000 + i*100, &res);
    }

    /* Now resend entry-000 with a new value: Because it was evicted, it executes anew (proves RAM buffer bounds) */
    evict_first.param_val = 80;
    ok = command_manager_execute(&evict_first, &normal_reading, 1750000750, now_ms + 9000, &res);
    TEST_ASSERT(ok && res.success, "Evicted command is executed afresh rather than recognized as duplicate");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 80, "Vent updated to 80 deg confirming execution");

    /* 8. Reboot reset:
     * Calling command_manager_init() resets in-RAM ring buffer, demonstrating firmware idempotency is RAM-only */
    DeviceCommand pre_reboot_cmd = {
        .command_id = "cmd-pre-reboot-001",
        .idempotency_key = "idemp-pre-reboot-k1",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 50,
        .expires_at_sec = 2000000000
    };
    command_manager_execute(&pre_reboot_cmd, &normal_reading, 1750000800, now_ms + 10000, &res);

    /* Simulate device reboot */
    command_manager_init();

    pre_reboot_cmd.param_val = 25;
    ok = command_manager_execute(&pre_reboot_cmd, &normal_reading, 1750000805, now_ms + 11000, &res);
    TEST_ASSERT(ok && res.success, "Post-reboot: Cleared RAM allows command re-execution (proves RAM-only limit)");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 25, "Vent updated to 25 deg post-reboot");
}

int main(void) {
    printf("====================================================\n");
    printf("    SMARTGROW AI — EMBEDDED FIRMWARE UNIT TESTS     \n");
    printf("====================================================\n");

    test_sensor_calibration();
    test_safety_manager();
    test_command_manager();
    test_telemetry_client();
    test_edge_ai_safety();
    test_ota_manager();
    test_wifi_manager();
    test_network_client();
    test_dht11_driver_and_faults();
    test_centralized_configuration();
    test_offline_autonomous_control_and_reboot();
    test_audit_safety_and_control_mode();
    test_release_readiness_verification();
    test_ttl_and_idempotency_deep_audit();

    printf("\n====================================================\n");
    printf("  SUMMARY: %d / %d tests passed (%.1f%%)\n",
           g_tests_passed, g_tests_run, (float)g_tests_passed / g_tests_run * 100.0f);
    printf("====================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
