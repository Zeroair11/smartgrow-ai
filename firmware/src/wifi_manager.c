/**
 * @file wifi_manager.c
 * @brief ESP32-S3 Wi-Fi Station Manager Implementation
 */

#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
static const char *TAG = "wifi_mgr";
#endif

#include "firmware_config.h"

static WifiManagerConfig s_config = {
    .ssid = DEFAULT_WIFI_SSID,
    .password = DEFAULT_WIFI_PASSWORD,
    .max_retry = DEFAULT_WIFI_MAX_RETRY,
    .initial_backoff_ms = DEFAULT_WIFI_INITIAL_BACKOFF_MS,
    .max_backoff_ms = DEFAULT_WIFI_MAX_BACKOFF_MS
};

static WifiManagerStatus s_status = {.state = WIFI_MGR_STATE_IDLE,
                                     .ip_address = "0.0.0.0",
                                     .mac_address = "00:00:00:00:00:00",
                                     .rssi = 0,
                                     .reconnect_count = 0,
                                     .current_backoff_ms = 1000,
                                     .last_disconnect_ms = 0,
                                     .has_ip = false};

static bool s_is_initialized = false;

#if defined(ESP_PLATFORM)
static esp_netif_t *s_sta_netif = NULL;
static esp_event_handler_instance_t s_wifi_handler_instance = NULL;
static esp_event_handler_instance_t s_ip_handler_instance = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  (void)arg;
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "Wi-Fi Station started, initiating connection...");
      s_status.state = WIFI_MGR_STATE_CONNECTING;
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "Wi-Fi associated with AP, awaiting DHCP IP lease...");
      s_status.state = WIFI_MGR_STATE_CONNECTED_WAITING_IP;
      break;

    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disc =
          (wifi_event_sta_disconnected_t *)event_data;
      ESP_LOGW(TAG, "Wi-Fi disconnected (reason: %d)", disc ? disc->reason : 0);
      s_status.has_ip = false;
      strncpy(s_status.ip_address, "0.0.0.0", sizeof(s_status.ip_address) - 1);
      s_status.state = WIFI_MGR_STATE_RECONNECT_BACKOFF;
      s_status.reconnect_count++;

      /* Exponential backoff calculation */
      if (s_status.current_backoff_ms < s_config.max_backoff_ms) {
        s_status.current_backoff_ms *= 2;
        if (s_status.current_backoff_ms > s_config.max_backoff_ms) {
          s_status.current_backoff_ms = s_config.max_backoff_ms;
        }
      }
      ESP_LOGI(TAG, "Next reconnect attempt in %u ms (attempt #%u)",
               (unsigned)s_status.current_backoff_ms,
               (unsigned)s_status.reconnect_count);
      break;
    }

    default:
      break;
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      snprintf(s_status.ip_address, sizeof(s_status.ip_address), IPSTR,
               IP2STR(&event->ip_info.ip));
      s_status.has_ip = true;
      s_status.state = WIFI_MGR_STATE_GOT_IP;
      s_status.current_backoff_ms = s_config.initial_backoff_ms;
      ESP_LOGI(TAG, "Wi-Fi ONLINE! Assigned IP: %s", s_status.ip_address);

      /* Read RSSI */
      wifi_ap_record_t ap_info;
      if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        s_status.rssi = ap_info.rssi;
      }
    }
  }
}
#endif /* ESP_PLATFORM */

bool wifi_manager_init(const WifiManagerConfig *config) {
  if (config) {
    s_config = *config;
    if (s_config.initial_backoff_ms == 0)
      s_config.initial_backoff_ms = 1000;
    if (s_config.max_backoff_ms == 0)
      s_config.max_backoff_ms = 30000;
  } else {
    const FirmwareConfig *fw_cfg = firmware_config_get();
    strncpy(s_config.ssid, fw_cfg->wifi.ssid, sizeof(s_config.ssid) - 1);
    s_config.ssid[sizeof(s_config.ssid) - 1] = '\0';
    strncpy(s_config.password, fw_cfg->wifi.password, sizeof(s_config.password) - 1);
    s_config.password[sizeof(s_config.password) - 1] = '\0';
    s_config.max_retry = fw_cfg->wifi.max_retry;
    s_config.initial_backoff_ms = fw_cfg->wifi.initial_backoff_ms;
    s_config.max_backoff_ms = fw_cfg->wifi.max_backoff_ms;
  }

  s_status.state = WIFI_MGR_STATE_INITIALIZING;
  s_status.has_ip = false;
  s_status.current_backoff_ms = s_config.initial_backoff_ms;
  s_status.reconnect_count = 0;
  strncpy(s_status.ip_address, "0.0.0.0", sizeof(s_status.ip_address) - 1);

#if defined(ESP_PLATFORM)
  /* Initialize NVS partition if not already initialized */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    s_status.state = WIFI_MGR_STATE_FAILED;
    return false;
  }

  /* Initialize TCP/IP stack */
  ESP_ERROR_CHECK(esp_netif_init());

  /* Create default event loop if not yet created */
  esp_err_t loop_err = esp_event_loop_create_default();
  if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to create default event loop: %s",
             esp_err_to_name(loop_err));
    return false;
  }

  /* Create default STA netif if not already created */
  if (!s_sta_netif) {
    s_sta_netif = esp_netif_create_default_wifi_sta();
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
    return false;
  }

  /* Register Event Handlers cleanly */
  if (!s_wifi_handler_instance) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
        &s_wifi_handler_instance));
  }
  if (!s_ip_handler_instance) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
        &s_ip_handler_instance));
  }

  wifi_config_t wifi_cfg = {0};
  strncpy((char *)wifi_cfg.sta.ssid, s_config.ssid,
          sizeof(wifi_cfg.sta.ssid) - 1);
  strncpy((char *)wifi_cfg.sta.password, s_config.password,
          sizeof(wifi_cfg.sta.password) - 1);
  wifi_cfg.sta.threshold.authmode =
      (strlen(s_config.password) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
  wifi_cfg.sta.pmf_cfg.capable = true;
  wifi_cfg.sta.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

  /* Read device MAC address */
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(s_status.mac_address, sizeof(s_status.mac_address),
             "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
  }
#else
  snprintf(s_status.mac_address, sizeof(s_status.mac_address), "%s", "b8:1f:3f:bb:d0:c4");
#endif

  s_is_initialized = true;
  s_status.state = WIFI_MGR_STATE_IDLE;
  return true;
}

bool wifi_manager_start(void) {
  if (!s_is_initialized)
    return false;

#if defined(ESP_PLATFORM)
  s_status.state = WIFI_MGR_STATE_CONNECTING;
  esp_err_t ret = esp_wifi_start();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
    s_status.state = WIFI_MGR_STATE_FAILED;
    return false;
  }
  return true;
#else
  s_status.state = WIFI_MGR_STATE_CONNECTING;
  return true;
#endif
}

void wifi_manager_stop(void) {
  s_status.has_ip = false;
  s_status.state = WIFI_MGR_STATE_DISCONNECTED;
#if defined(ESP_PLATFORM)
  esp_wifi_disconnect();
  esp_wifi_stop();
#endif
}

bool wifi_manager_is_connected(void) {
  /* Critical Acceptance Criterion: Never report connected/online before valid
   * IP */
  return (s_status.has_ip && s_status.state == WIFI_MGR_STATE_GOT_IP);
}

const WifiManagerStatus *wifi_manager_get_status(void) { return &s_status; }

void wifi_manager_tick(uint32_t current_time_ms) {
  if (s_status.state == WIFI_MGR_STATE_RECONNECT_BACKOFF) {
    if (s_status.last_disconnect_ms == 0) {
      s_status.last_disconnect_ms = current_time_ms;
    }

    uint32_t elapsed = current_time_ms - s_status.last_disconnect_ms;
    if (elapsed >= s_status.current_backoff_ms) {
      s_status.last_disconnect_ms = current_time_ms;
      s_status.state = WIFI_MGR_STATE_CONNECTING;
#if defined(ESP_PLATFORM)
      ESP_LOGI(TAG, "Executing reconnection backoff attempt...");
      esp_wifi_connect();
#endif
    }
  }
}

#if !defined(ESP_PLATFORM)
void wifi_manager_mock_trigger_event(int event, const char *mock_ip) {
  switch (event) {
  case 1: /* STA_START */
    s_status.state = WIFI_MGR_STATE_CONNECTING;
    break;
  case 2: /* STA_CONNECTED */
    s_status.state = WIFI_MGR_STATE_CONNECTED_WAITING_IP;
    s_status.has_ip = false;
    break;
  case 3: /* GOT_IP */
    s_status.state = WIFI_MGR_STATE_GOT_IP;
    s_status.has_ip = true;
    if (mock_ip) {
      strncpy(s_status.ip_address, mock_ip, sizeof(s_status.ip_address) - 1);
    } else {
      strncpy(s_status.ip_address, "192.168.1.150",
              sizeof(s_status.ip_address) - 1);
    }
    s_status.current_backoff_ms = s_config.initial_backoff_ms;
    break;
  case 4: /* STA_DISCONNECTED */
    s_status.has_ip = false;
    strncpy(s_status.ip_address, "0.0.0.0", sizeof(s_status.ip_address) - 1);
    s_status.state = WIFI_MGR_STATE_RECONNECT_BACKOFF;
    s_status.reconnect_count++;
    if (s_status.current_backoff_ms < s_config.max_backoff_ms) {
      s_status.current_backoff_ms *= 2;
      if (s_status.current_backoff_ms > s_config.max_backoff_ms) {
        s_status.current_backoff_ms = s_config.max_backoff_ms;
      }
    }
    break;
  default:
    break;
  }
}
#endif
