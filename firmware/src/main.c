/**
 * @file main.c
 * @brief SmartGrow AI — ESP32-S3 Application Entry Point & FreeRTOS Orchestration
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "actuator_manager.h"
#include "board_config.h"
#include "command_manager.h"
#include "edge_ai.h"
#include "firmware_config.h"
#include "network_client.h"
#include "ota_manager.h"
#include "safety_manager.h"
#include "sensor_calibration.h"
#include "sensor_manager.h"
#include "telemetry_client.h"
#include "wifi_manager.h"

#if defined(ESP_PLATFORM)
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

static const char *TAG = "smartgrow_main";

/* Global Shared State with Mutex Protection */
static SensorReadings s_latest_readings = {0};
static EdgeAIResult s_latest_ai = {0};
static char s_boot_id[48] = "boot-0001";
static char s_device_id[48] = DEFAULT_DEVICE_ID;

#if defined(ESP_PLATFORM)
static SemaphoreHandle_t s_state_mutex = NULL;

static uint32_t get_uptime_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
#else
static uint32_t s_simulated_uptime_ms = 0;
static uint32_t get_uptime_ms(void) { return s_simulated_uptime_ms; }
#endif

/**
 * @brief High-Priority Task: Sensor Acquisition, Safety Guardrails & Local Edge AI
 * Executes at high priority (5) to guarantee hard real-time actuator cutoff within 100ms
 */
#if defined(ESP_PLATFORM)
static void safety_sensor_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI(TAG, "Safety & Sensor task launched (Priority: High, Core 0)");

  const FirmwareConfig *cfg = firmware_config_get();
  TickType_t last_wake_time = xTaskGetTickCount();
  const TickType_t tick_interval = pdMS_TO_TICKS(100); /* 100ms loop */

  /* Calculate sample ticks from centralized DHT11 sample interval (default 2000ms = 20 ticks) */
  uint32_t required_ticks = cfg->dht11.sample_interval_ms / 100;
  if (required_ticks == 0) required_ticks = 20;
  uint32_t sensor_sample_counter = 0;

  while (1) {
    uint32_t now_ms = get_uptime_ms();

    /* 1. Hard Real-Time Safety Manager Cutoff Watchdog */
    bool emergency_trip = safety_manager_tick(now_ms);
    if (emergency_trip) {
      ESP_LOGW(TAG, "EMERGENCY SAFETY WATCHDOG CUTOFF: Pump forced OFF!");
    }

    /* 2. Sensor Acquisition & Local AI Inference */
    sensor_sample_counter++;
    if (sensor_sample_counter >= required_ticks) {
      sensor_sample_counter = 0;

      SensorReadings readings;
      bool read_ok = sensor_manager_read_all_ex(&readings, now_ms);

      if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_latest_readings = readings;

        if (read_ok) {
          /* Execute Edge AI TinyML Decision Tree Inference (Zero-heap, runs on-device) */
          EdgeAIResult ai_res;
          bool ai_ok = edge_ai_infer(&readings, now_ms, &ai_res);
          if (ai_ok) {
            s_latest_ai = ai_res;

            ControlMode mode = cfg->control.mode;
            /* Autonomous Execution: Runs even if server/backend is completely offline */
            if (mode == CONTROL_MODE_AUTO || mode == CONTROL_MODE_HYBRID) {
              if (ai_res.recommended_pump_action) {
                SafetyStatus reason = SAFETY_OK;
                uint32_t planned_run_ms = cfg->control.local_rule.pump_duration_s * 1000;
                if (planned_run_ms == 0) planned_run_ms = 30000; /* Default 30s target */
                bool started = safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, planned_run_ms,
                                                              &readings, now_ms, &reason);
                if (!started) {
                  ESP_LOGW(TAG, "Edge AI pump request BLOCKED by Safety Gate: code %d", (int)reason);
                }
              } else if (mode == CONTROL_MODE_HYBRID) {
                /* HYBRID Fallback: When Edge AI does not recommend irrigation, fall back to local rule */
                if (cfg->control.local_rule.enabled && readings.soil_valid) {
                  if (readings.soil_moisture_pct < cfg->control.local_rule.soil_moisture_threshold_pct) {
                    SafetyStatus reason = SAFETY_OK;
                    uint32_t planned_run_ms = cfg->control.local_rule.pump_duration_s * 1000;
                    if (planned_run_ms == 0) planned_run_ms = 30000;
                    safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, planned_run_ms,
                                                   &readings, now_ms, &reason);
                  }
                }
              }
            } else if (mode == CONTROL_MODE_RULE) {
              if (cfg->control.local_rule.enabled && readings.soil_valid) {
                if (readings.soil_moisture_pct < cfg->control.local_rule.soil_moisture_threshold_pct) {
                  SafetyStatus reason = SAFETY_OK;
                  uint32_t planned_run_ms = cfg->control.local_rule.pump_duration_s * 1000;
                  if (planned_run_ms == 0) planned_run_ms = 30000;
                  safety_manager_request_pump_on(SAFETY_SRC_LOCAL_RULE, planned_run_ms,
                                                 &readings, now_ms, &reason);
                }
              }
            } else if (mode == CONTROL_MODE_SAFE) {
              if (safety_manager_get_state()->pump_is_running) {
                safety_manager_request_pump_off(SAFETY_SRC_WATCHDOG, "SAFE_MODE_ENFORCED", now_ms);
              }
            }
          }
        } else {
          /* Sensor acquisition fault: evaluate AI with faulted readings to ensure AI predicts SENSOR_FAULT */
          EdgeAIResult ai_res;
          edge_ai_infer(&readings, now_ms, &ai_res);
          s_latest_ai = ai_res;
          /* Fail-safe: ensure pump is held OFF via Safety Gate */
          safety_manager_request_pump_off(SAFETY_SRC_WATCHDOG, "SENSOR_FAULT_FAILSAFE", now_ms);
          ESP_LOGW(TAG, "Sensor acquisition fault (DHT11 or Soil Moisture unplausible); pump held OFF");
        }
        xSemaphoreGive(s_state_mutex);
      }
    }

    vTaskDelayUntil(&last_wake_time, tick_interval);
  }
}

/**
 * @brief Medium-Priority Task: Network Connectivity, Telemetry Uplink & Command Downlink
 */
static void network_uplink_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI(TAG, "Network Uplink task launched (Priority: Medium, Core 1)");

  const FirmwareConfig *cfg = firmware_config_get();
  uint32_t last_telemetry_ms = 0;
  uint32_t last_command_poll_ms = 0;
  const uint32_t telemetry_interval_ms = cfg->network.telemetry_interval_ms;
  const uint32_t command_poll_interval_ms = cfg->network.command_poll_interval_ms;

  while (1) {
    uint32_t now_ms = get_uptime_ms();

    /* 1. Wi-Fi Reconnection Backoff Handler */
    wifi_manager_tick(now_ms);

    /* 2. Process Network Operations if Connected with Valid IP */
    if (wifi_manager_is_connected()) {
      const WifiManagerStatus *net_stat = wifi_manager_get_status();

      /* Device Registration with Backend */
      const NetworkClientStats *cli_stats = network_client_get_stats();
      if (!cli_stats->is_registered) {
        ESP_LOGI(TAG, "Enrolling device '%s' with backend...", s_device_id);
        if (network_client_register_device()) {
          ESP_LOGI(TAG, "Device successfully registered with backend!");
        } else {
          ESP_LOGW(TAG, "Device registration failed; retrying next cycle");
        }
      }

      /* Telemetry Uplink */
      if (now_ms - last_telemetry_ms >= telemetry_interval_ms) {
        last_telemetry_ms = now_ms;

        SensorReadings curr_readings;
        EdgeAIResult curr_ai;
        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          curr_readings = s_latest_readings;
          curr_ai = s_latest_ai;
          xSemaphoreGive(s_state_mutex);
        }

        const ActuatorState *act_state = actuator_manager_get_state();

        TelemetryPayload payload = {
            .schema_version = cfg->device.schema_version,
            .device_id = s_device_id,
            .boot_id = s_boot_id,
            .sequence = telemetry_client_next_sequence(),
            .timestamp_ms = now_ms,
            .soil_moisture_pct = curr_readings.soil_moisture_pct,
            .temperature_c = curr_readings.temperature_c,
            .humidity_pct = curr_readings.humidity_pct,
            .light_level_pct = curr_readings.light_level_pct,
            .soil_valid = curr_readings.soil_valid,
            .dht_valid = curr_readings.dht_valid,
            .light_valid = curr_readings.light_valid,
            .ai_class = curr_ai.predicted_class,
            .ai_confidence = curr_ai.confidence,
            .model_version = cfg->ai.model_version,
            .firmware_version = cfg->device.firmware_version,
            .pump_state = act_state ? act_state->pump_active : false,
            .operating_mode = (cfg->control.mode == CONTROL_MODE_SAFE) ? "SAFE" :
                              (cfg->control.mode == CONTROL_MODE_MANUAL) ? "MANUAL" :
                              (cfg->control.mode == CONTROL_MODE_RULE) ? "RULE" :
                              (cfg->control.mode == CONTROL_MODE_HYBRID) ? "HYBRID" : "AUTO",
            .health_status = (curr_readings.soil_valid && curr_readings.dht_valid)
                                ? "HEALTHY"
                                : "SENSOR_FAULT"};

        if (network_client_send_telemetry(&payload)) {
          char soil_str[16];
          char temp_str[16];
          char hum_str[16];

          if (payload.soil_valid && !isnan(payload.soil_moisture_pct)) {
            snprintf(soil_str, sizeof(soil_str), "%.1f%%", payload.soil_moisture_pct);
          } else {
            snprintf(soil_str, sizeof(soil_str), "ERR");
          }

          if (payload.dht_valid && !isnan(payload.temperature_c)) {
            snprintf(temp_str, sizeof(temp_str), "%.1fC", payload.temperature_c);
          } else {
            snprintf(temp_str, sizeof(temp_str), "ERR");
          }

          if (payload.dht_valid && !isnan(payload.humidity_pct)) {
            snprintf(hum_str, sizeof(hum_str), "%.1f%%", payload.humidity_pct);
          } else {
            snprintf(hum_str, sizeof(hum_str), "ERR");
          }

          ESP_LOGI(TAG,
                   "Telemetry uploaded [Seq: %u, Soil: %s, Temp: %s, Hum: %s, RSSI: %d dBm]",
                   (unsigned)payload.sequence, soil_str, temp_str, hum_str, net_stat->rssi);
        } else {
          ESP_LOGW(TAG, "Telemetry upload failed");
        }
      }

      /* Command Downlink Polling */
      if (now_ms - last_command_poll_ms >= command_poll_interval_ms) {
        last_command_poll_ms = now_ms;

        DeviceCommand commands[4];
        size_t cmd_count = 0;
        if (network_client_poll_commands(commands, 4, &cmd_count) && cmd_count > 0) {
          ESP_LOGI(TAG, "Received %u downlink command(s) from backend", (unsigned)cmd_count);

          for (size_t i = 0; i < cmd_count; i++) {
            SensorReadings curr_readings;
            if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
              curr_readings = s_latest_readings;
              xSemaphoreGive(s_state_mutex);
            }

            CommandExecutionResult exec_result;
            time_t now_epoch = time(NULL);
            uint32_t epoch_sec = (now_epoch > 1000000000) ? (uint32_t)now_epoch : 0;
            bool ok = command_manager_execute(&commands[i], &curr_readings,
                                              epoch_sec, now_ms, &exec_result);
            (void)ok;

            ESP_LOGI(TAG, "Command %s: Status=%s, Code=%d, Msg='%s'",
                     commands[i].command_id, exec_result.status,
                     exec_result.error_code, exec_result.message);

            if (network_client_send_command_ack(&exec_result)) {
              ESP_LOGI(TAG, "ACK dispatched successfully for %s", commands[i].command_id);
            }

            /* If command was system restart, execute delayed reboot now that ACK is sent */
            if (commands[i].type == CMD_TYPE_RESTART && exec_result.success) {
              ESP_LOGW(TAG, "Rebooting system in 1000ms upon command request...");
              vTaskDelay(pdMS_TO_TICKS(1000));
              esp_restart();
            }
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

#if defined(CONFIG_SMARTGROW_SERIAL_HIL)
/**
 * @brief Physical HIL Serial Protocol Task: Listens for PING / Heartbeats from HIL runner
 */
static void serial_hil_task(void *pvParameters) {
  (void)pvParameters;
  uint8_t rx_buf[128];

  const uart_port_t uart_num = UART_NUM_0;
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity    = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  esp_err_t err = uart_param_config(uart_num, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure UART0 for serial HIL: %s", esp_err_to_name(err));
    vTaskDelete(NULL);
    return;
  }
  err = uart_driver_install(uart_num, 256, 256, 0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install UART0 driver for serial HIL: %s", esp_err_to_name(err));
    vTaskDelete(NULL);
    return;
  }

  while (1) {
    int len = uart_read_bytes(uart_num, rx_buf, sizeof(rx_buf) - 1,
                              pdMS_TO_TICKS(200));
    if (len > 0) {
      rx_buf[len] = '\0';
      if (strstr((char *)rx_buf, "PING") != NULL) {
        const char *pong_reply = "PONG ESP32-S3 READY\n";
        uart_write_bytes(uart_num, pong_reply, strlen(pong_reply));
      }
    } else if (len < 0) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#endif
#endif /* ESP_PLATFORM */

/**
 * @brief Application Main Entry Point (ESP-IDF)
 */
void app_main(void) {
  printf("====================================================\n");
  printf("       SMARTGROW AI — ESP32-S3 EMBEDDED FIRMWARE    \n");
  printf("  Board: " BOARD_NAME " | Version: " FIRMWARE_VERSION "\n");
  printf("  Build: " FIRMWARE_BUILD_DATE "\n");
  printf("====================================================\n");

#if defined(ESP_PLATFORM)
  /* 0. Initialize Non-Volatile Storage (NVS) */
  firmware_config_nvs_init();

  /* 1. Initialize Centralized Configuration (restores NVS ControlMode) */
  if (!firmware_config_init(NULL)) {
    ESP_LOGE(TAG, "FATAL: Centralized configuration failed validation! Halting boot.");
    return;
  }
  const FirmwareConfig *cfg = firmware_config_get();

  /* 2. Actuator Hardware Safe Reset (Relay forced OFF immediately at boot) */
  actuator_manager_init();

  /* 3. Derive unique Device ID and Boot ID */
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(s_device_id, sizeof(s_device_id), "esp32s3-%02x%02x%02x", mac[3], mac[4], mac[5]);
    firmware_config_set_device_id(s_device_id);
  } else {
    strncpy(s_device_id, cfg->device.device_id, sizeof(s_device_id) - 1);
  }
  snprintf(s_boot_id, sizeof(s_boot_id), "boot-%lu", (unsigned long)esp_random());

  /* 4. Display safe masked configuration summary */
  firmware_config_log_safe(cfg);

  /* 5. Initialize Subsystems using Centralized Configuration */
  s_state_mutex = xSemaphoreCreateMutex();

  safety_manager_init(cfg->safety.max_pump_duration_ms, cfg->safety.min_pump_cooldown_ms);
  command_manager_init();
  telemetry_client_init(s_device_id, s_boot_id);
  edge_ai_init();
  ota_manager_init(cfg->device.firmware_version);

  SoilCalibrationConfig calib_cfg = {
      .adc_dry_raw = cfg->sensors.soil_adc_dry_raw,
      .adc_wet_raw = cfg->sensors.soil_adc_wet_raw,
      .vref_mv = cfg->sensors.adc_vref_mv,
      .is_calibrated = false
  };
  sensor_manager_init(&calib_cfg);

  /* 6. Initialize Wi-Fi Manager from Centralized Config */
  WifiManagerConfig wifi_cfg = {
      .max_retry = cfg->wifi.max_retry,
      .initial_backoff_ms = cfg->wifi.initial_backoff_ms,
      .max_backoff_ms = cfg->wifi.max_backoff_ms
  };
  strncpy(wifi_cfg.ssid, cfg->wifi.ssid, sizeof(wifi_cfg.ssid) - 1);
  strncpy(wifi_cfg.password, cfg->wifi.password, sizeof(wifi_cfg.password) - 1);
  wifi_manager_init(&wifi_cfg);
  wifi_manager_start();

  /* 7. Initialize Network Client from Centralized Config */
  NetworkClientConfig net_cfg = {
      .timeout_ms = cfg->network.http_timeout_ms
  };
  strncpy(net_cfg.base_url, cfg->network.base_url, sizeof(net_cfg.base_url) - 1);
  strncpy(net_cfg.device_id, s_device_id, sizeof(net_cfg.device_id) - 1);
  strncpy(net_cfg.registration_token, cfg->network.registration_token, sizeof(net_cfg.registration_token) - 1);
  strncpy(net_cfg.firmware_version, cfg->device.firmware_version, sizeof(net_cfg.firmware_version) - 1);
  strncpy(net_cfg.model_version, cfg->ai.model_version, sizeof(net_cfg.model_version) - 1);
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(net_cfg.mac_address, sizeof(net_cfg.mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    snprintf(net_cfg.mac_address, sizeof(net_cfg.mac_address), "%s", "00:00:00:00:00:00");
  }
  network_client_init(&net_cfg);

  /* 8. Launch FreeRTOS Operational Tasks */
  xTaskCreatePinnedToCore(safety_sensor_task, "safety_sensor", 4096, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(network_uplink_task, "network_uplink", 6144, NULL, 3, NULL, 1);
#if defined(CONFIG_SMARTGROW_SERIAL_HIL)
  xTaskCreatePinnedToCore(serial_hil_task, "serial_hil", 2048, NULL, 2, NULL, 0);
#endif

  ESP_LOGI(TAG, "All FreeRTOS operational tasks successfully spawned.");
#else
  printf("[Native Host Simulation] app_main initialized.\n");
#endif
}
