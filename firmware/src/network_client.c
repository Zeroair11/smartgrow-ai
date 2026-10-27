/**
 * @file network_client.c
 * @brief REST HTTP/HTTPS Uplink Client Implementation
 */

#include "network_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firmware_config.h"

#if defined(ESP_PLATFORM)
#include "esp_http_client.h"
#include "esp_log.h"
static const char *TAG = "net_client";
#endif

static NetworkClientConfig s_config = {
    .base_url = "http://" DEFAULT_BACKEND_HOST ":8000" DEFAULT_API_BASE_PATH,
    .device_id = DEFAULT_DEVICE_ID,
    .registration_token = DEFAULT_REGISTRATION_TOKEN,
    .firmware_version = DEFAULT_FIRMWARE_VERSION,
    .model_version = DEFAULT_AI_MODEL_VERSION,
    .mac_address = "00:00:00:00:00:00",
    .timeout_ms = DEFAULT_HTTP_TIMEOUT_MS};

static NetworkClientStats s_stats = {.is_registered = false,
                                     .telemetry_sent_count = 0,
                                     .telemetry_fail_count = 0,
                                     .commands_polled_count = 0,
                                     .acks_sent_count = 0,
                                     .last_http_status = 0};

#if !defined(ESP_PLATFORM)
static int s_mock_http_status = 200;
static char s_mock_response_body[2048] = "[]";
static char s_last_post_body[1024] = {0};
static char s_last_url[256] = {0};
#endif

bool network_client_init(const NetworkClientConfig *config) {
  if (config) {
    s_config = *config;
    if (s_config.timeout_ms == 0)
      s_config.timeout_ms = 5000;
  } else {
    const FirmwareConfig *fw_cfg = firmware_config_get();
    strncpy(s_config.base_url, fw_cfg->network.base_url, sizeof(s_config.base_url) - 1);
    s_config.base_url[sizeof(s_config.base_url) - 1] = '\0';
    strncpy(s_config.device_id, fw_cfg->device.device_id, sizeof(s_config.device_id) - 1);
    s_config.device_id[sizeof(s_config.device_id) - 1] = '\0';
    strncpy(s_config.registration_token, fw_cfg->network.registration_token, sizeof(s_config.registration_token) - 1);
    s_config.registration_token[sizeof(s_config.registration_token) - 1] = '\0';
    strncpy(s_config.firmware_version, fw_cfg->device.firmware_version, sizeof(s_config.firmware_version) - 1);
    s_config.firmware_version[sizeof(s_config.firmware_version) - 1] = '\0';
    strncpy(s_config.model_version, fw_cfg->ai.model_version, sizeof(s_config.model_version) - 1);
    s_config.model_version[sizeof(s_config.model_version) - 1] = '\0';
    s_config.timeout_ms = fw_cfg->network.http_timeout_ms;
  }
  s_stats.is_registered = false;
  s_stats.telemetry_sent_count = 0;
  s_stats.telemetry_fail_count = 0;
  s_stats.commands_polled_count = 0;
  s_stats.acks_sent_count = 0;
  s_stats.last_http_status = 0;
  return true;
}

static int http_perform_request(const char *url, const char *method,
                                const char *post_data, char *out_response,
                                size_t response_buf_size) {
  if (!url || !method)
    return -1;

#if defined(ESP_PLATFORM)
  esp_http_client_config_t http_cfg = {
      .url = url,
      .timeout_ms = (int)s_config.timeout_ms,
      .buffer_size = 2048,
  };

  esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
  if (!client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return -1;
  }

  if (strcmp(method, "POST") == 0) {
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (post_data) {
      esp_http_client_set_post_field(client, post_data, (int)strlen(post_data));
    }
  } else {
    esp_http_client_set_method(client, HTTP_METHOD_GET);
  }

  esp_err_t err =
      esp_http_client_open(client, post_data ? (int)strlen(post_data) : 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return -1;
  }

  if (post_data && strcmp(method, "POST") == 0) {
    int wlen = esp_http_client_write(client, post_data, (int)strlen(post_data));
    if (wlen < 0) {
      ESP_LOGE(TAG, "Failed to write HTTP post data");
      esp_http_client_cleanup(client);
      return -1;
    }
  }

  esp_http_client_fetch_headers(client);
  int status_code = esp_http_client_get_status_code(client);
  s_stats.last_http_status = status_code;

  if (out_response && response_buf_size > 0) {
    int rlen = esp_http_client_read_response(client, out_response,
                                             (int)response_buf_size - 1);
    if (rlen >= 0) {
      out_response[rlen] = '\0';
    } else {
      out_response[0] = '\0';
    }
  }

  esp_http_client_cleanup(client);
  return status_code;
#else
  strncpy(s_last_url, url, sizeof(s_last_url) - 1);
  if (post_data) {
    strncpy(s_last_post_body, post_data, sizeof(s_last_post_body) - 1);
  } else {
    s_last_post_body[0] = '\0';
  }

  if (out_response && response_buf_size > 0) {
    strncpy(out_response, s_mock_response_body, response_buf_size - 1);
    out_response[response_buf_size - 1] = '\0';
  }

  s_stats.last_http_status = s_mock_http_status;
  return s_mock_http_status;
#endif
}

bool network_client_register_device(void) {
  char url[256];
  snprintf(url, sizeof(url), "%s/devices/register", s_config.base_url);

  char payload[512];
  snprintf(payload, sizeof(payload),
           "{"
           "\"device_id\":\"%s\","
           "\"name\":\"%s\","
           "\"registration_token\":\"%s\","
           "\"firmware_version\":\"%s\","
           "\"model_version\":\"%s\","
           "\"mac_address\":\"%s\""
           "}",
           s_config.device_id, s_config.device_id, s_config.registration_token,
           s_config.firmware_version, s_config.model_version,
           s_config.mac_address);

  int status = http_perform_request(url, "POST", payload, NULL, 0);
  if (status == 200 || status == 201) {
    s_stats.is_registered = true;
    return true;
  }
  s_stats.is_registered = false;
  return false;
}

bool network_client_send_telemetry(const TelemetryPayload *payload) {
  if (!payload)
    return false;

  char url[256];
  snprintf(url, sizeof(url), "%s/telemetry", s_config.base_url);

  char json_buf[1024];
  int len =
      telemetry_client_serialize_json(payload, json_buf, sizeof(json_buf));
  if (len <= 0)
    return false;

  int status = http_perform_request(url, "POST", json_buf, NULL, 0);
  if (status == 200 || status == 201) {
    s_stats.telemetry_sent_count++;
    return true;
  } else {
    s_stats.telemetry_fail_count++;
    return false;
  }
}

/* Minimal robust JSON helper for command parser */
static bool extract_json_string(const char *json, const char *key, char *out,
                                size_t out_size) {
  char search_pattern[64];
  snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"", key);
  const char *start = strstr(json, search_pattern);
  if (!start) {
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\": \"", key);
    start = strstr(json, search_pattern);
  }
  if (!start)
    return false;

  start += strlen(search_pattern);
  const char *end = strchr(start, '\"');
  if (!end)
    return false;

  size_t len = (size_t)(end - start);
  if (len >= out_size)
    len = out_size - 1;
  strncpy(out, start, len);
  out[len] = '\0';
  return true;
}

static bool extract_json_int(const char *json, const char *key,
                             uint32_t *out_val) {
  char search_pattern[64];
  snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
  const char *start = strstr(json, search_pattern);
  if (!start)
    return false;

  start += strlen(search_pattern);
  while (*start == ' ' || *start == '\t')
    start++;
  if (*start < '0' || *start > '9')
    return false;

  *out_val = (uint32_t)strtoul(start, NULL, 10);
  return true;
}

bool network_client_poll_commands(DeviceCommand *out_commands,
                                  size_t max_commands, size_t *out_count) {
  if (out_count)
    *out_count = 0;
  if (!out_commands || max_commands == 0)
    return false;

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/poll-commands", s_config.base_url,
           s_config.device_id);

  char response[2048] = {0};
  int status =
      http_perform_request(url, "GET", NULL, response, sizeof(response));
  if (status != 200) {
    return false;
  }

  /* Response is JSON array of commands */
  size_t count = 0;
  const char *cur = response;
  while ((cur = strstr(cur, "{\"")) != NULL && count < max_commands) {
    const char *obj_end = strchr(cur, '}');
    if (!obj_end)
      break;

    size_t obj_len = (size_t)(obj_end - cur + 1);
    char obj_buf[512];
    if (obj_len < sizeof(obj_buf)) {
      strncpy(obj_buf, cur, obj_len);
      obj_buf[obj_len] = '\0';

      DeviceCommand *cmd = &out_commands[count];
      memset(cmd, 0, sizeof(*cmd));

      /* Extract command_id */
      if (extract_json_string(obj_buf, "command_id", cmd->command_id,
                              sizeof(cmd->command_id))) {
        char type_str[32] = {0};
        if (extract_json_string(obj_buf, "command_type", type_str,
                                sizeof(type_str))) {
          if (strcmp(type_str, "PUMP_START") == 0)
            cmd->type = CMD_TYPE_PUMP_START;
          else if (strcmp(type_str, "PUMP_STOP") == 0)
            cmd->type = CMD_TYPE_PUMP_STOP;
          else if (strcmp(type_str, "VENT_SET") == 0)
            cmd->type = CMD_TYPE_VENT_SET;
          else if (strcmp(type_str, "BUZZER_BEEP") == 0)
            cmd->type = CMD_TYPE_BUZZER_BEEP;
          else if (strcmp(type_str, "RESTART") == 0 ||
                   strcmp(type_str, "SYSTEM_RESTART") == 0)
            cmd->type = CMD_TYPE_RESTART;
          else if (strcmp(type_str, "CONFIG_UPDATE") == 0 ||
                   strcmp(type_str, "SET_MODE") == 0)
            cmd->type = CMD_TYPE_CONFIG_UPDATE;
          else
            cmd->type = CMD_TYPE_UNKNOWN;
        }

        /* Extract optional idempotency_key */
        extract_json_string(obj_buf, "idempotency_key", cmd->idempotency_key,
                            sizeof(cmd->idempotency_key));

        uint32_t val = 0;
        if (extract_json_int(obj_buf, "param_val", &val) ||
            extract_json_int(obj_buf, "duration_s", &val)) {
          cmd->param_val = val;
        } else {
          cmd->param_val = 30; /* default */
        }

        uint32_t exp_sec = 0;
        if (extract_json_int(obj_buf, "expires_at_sec", &exp_sec)) {
          cmd->expires_at_sec = exp_sec;
        } else {
          cmd->expires_at_sec = 0; /* Defer to server sweep if not specified */
        }
        count++;
      }
    }
    cur = obj_end + 1;
  }

  if (out_count)
    *out_count = count;
  s_stats.commands_polled_count += count;
  return true;
}

bool network_client_send_command_ack(const CommandExecutionResult *result) {
  if (!result)
    return false;

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/commands/ack", s_config.base_url,
           s_config.device_id);

  char payload[512];
  snprintf(payload, sizeof(payload),
           "{"
           "\"command_id\":\"%s\","
           "\"status\":\"%s\","
           "\"result_message\":\"%s\","
           "\"error_code\":%d"
           "}",
           result->command_id,
           result->status ? result->status
                          : (result->success ? "completed" : "failed"),
           result->message, result->error_code);

  int status = http_perform_request(url, "POST", payload, NULL, 0);
  if (status == 200) {
    s_stats.acks_sent_count++;
    return true;
  }
  return false;
}

const NetworkClientStats *network_client_get_stats(void) { return &s_stats; }

#if !defined(ESP_PLATFORM)
void network_client_mock_set_response(int http_code, const char *body) {
  s_mock_http_status = http_code;
  if (body) {
    strncpy(s_mock_response_body, body, sizeof(s_mock_response_body) - 1);
    s_mock_response_body[sizeof(s_mock_response_body) - 1] = '\0';
  } else {
    s_mock_response_body[0] = '\0';
  }
}

const char *network_client_mock_get_last_post_body(void) {
  return s_last_post_body;
}

const char *network_client_mock_get_last_url(void) { return s_last_url; }
#endif
