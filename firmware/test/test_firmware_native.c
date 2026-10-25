#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "board_config.h"
#include "sensor_calibration.h"
#include "sensor_manager.h"
#include "safety_manager.h"
#include "actuator_manager.h"
#include "command_manager.h"
#include "telemetry_client.h"

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

/* Test 1: Sensor Calibration */
static void test_sensor_calibration(void) {
    printf("\n--- Running Test Suite: Sensor Calibration ---\n");
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
    TEST_ASSERT(sensor_calibration_raw_to_moisture_pct(&cfg, 4000) == 0.0f, "Clamped to 0% when above dry");
    TEST_ASSERT(sensor_calibration_raw_to_moisture_pct(&cfg, 500) == 100.0f, "Clamped to 100% when below wet");
}

/* Test 2: Safety Manager Interlocks */
static void test_safety_manager(void) {
    printf("\n--- Running Test Suite: Safety Manager Guardrails ---\n");
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

    /* Immediate restart attempt should be BLOCKED due to cooldown */
    can_start = safety_manager_can_start_pump(&valid_readings, 125000, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_COOLDOWN_ACTIVE, "Pump start blocked by active cooldown");

    /* Sensor fault interlock */
    SensorReadings faulty_readings = valid_readings;
    faulty_readings.soil_valid = false;
    /* Advance time past cooldown */
    can_start = safety_manager_can_start_pump(&faulty_readings, 400000, &reason);
    TEST_ASSERT(!can_start && reason == SAFETY_ERR_SENSOR_FAULT, "Pump start blocked by sensor fault");
}

/* Test 3: Command Manager & Idempotency */
static void test_command_manager(void) {
    printf("\n--- Running Test Suite: Command Manager & Idempotency ---\n");
    command_manager_init();
    safety_manager_init(120000, 180000);

    DeviceCommand cmd = {
        .command_id = "cmd-uuid-001",
        .type = CMD_TYPE_VENT_SET,
        .param_val = 45,
        .expires_at_sec = 2000000000
    };

    CommandExecutionResult res;
    bool ok = command_manager_execute(&cmd, 1700000000, 10000, &res);
    TEST_ASSERT(ok && res.success && res.error_code == 0, "Vent command executed successfully");
    TEST_ASSERT(actuator_manager_get_state()->vent_servo_angle_deg == 45, "Vent angle set to 45 deg");

    /* Idempotent re-execution of same command_id */
    ok = command_manager_execute(&cmd, 1700000000, 10500, &res);
    TEST_ASSERT(ok && res.success, "Duplicate command safely handled with idempotent ACK");

    /* Expired command rejection */
    DeviceCommand expired_cmd = {
        .command_id = "cmd-uuid-002",
        .type = CMD_TYPE_PUMP_START,
        .param_val = 30,
        .expires_at_sec = 1600000000 /* expired in the past */
    };
    ok = command_manager_execute(&expired_cmd, 1700000000, 11000, &res);
    TEST_ASSERT(!ok && !res.success && res.error_code == 101, "Expired command rejected with error code 101");
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
    TEST_ASSERT(strstr(buf, "\"ai_class\":1") != NULL, "JSON contains ai_class");
}

/* Test 5: Edge AI and Safety Override */
#include "edge_ai.h"

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

int main(void) {
    printf("====================================================\n");
    printf("    SMARTGROW AI — EMBEDDED FIRMWARE UNIT TESTS     \n");
    printf("====================================================\n");

    test_sensor_calibration();
    test_safety_manager();
    test_command_manager();
    test_telemetry_client();
    test_edge_ai_safety();

    printf("\n====================================================\n");
    printf("  SUMMARY: %d / %d tests passed (%.1f%%)\n",
           g_tests_passed, g_tests_run, (float)g_tests_passed / g_tests_run * 100.0f);
    printf("====================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
