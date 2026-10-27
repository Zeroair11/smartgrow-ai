/**
 * @file wifi_manager.h
 * @brief ESP32-S3 Wi-Fi Station Manager with Exponential Backoff & State Machine
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_MGR_STATE_IDLE = 0,
    WIFI_MGR_STATE_INITIALIZING,
    WIFI_MGR_STATE_CONNECTING,
    WIFI_MGR_STATE_CONNECTED_WAITING_IP,
    WIFI_MGR_STATE_GOT_IP,
    WIFI_MGR_STATE_DISCONNECTED,
    WIFI_MGR_STATE_RECONNECT_BACKOFF,
    WIFI_MGR_STATE_FAILED
} WifiManagerState;

typedef struct {
    char ssid[33];
    char password[65];
    uint32_t max_retry;          /* 0 = infinite retry */
    uint32_t initial_backoff_ms; /* default 1000ms */
    uint32_t max_backoff_ms;     /* default 30000ms */
} WifiManagerConfig;

typedef struct {
    WifiManagerState state;
    char ip_address[16];
    char mac_address[18];
    int8_t rssi;
    uint32_t reconnect_count;
    uint32_t current_backoff_ms;
    uint32_t last_disconnect_ms;
    bool has_ip;
} WifiManagerStatus;

/**
 * @brief Initialize the Wi-Fi Station driver and event loops
 */
bool wifi_manager_init(const WifiManagerConfig *config);

/**
 * @brief Trigger connection to the configured Wi-Fi AP
 */
bool wifi_manager_start(void);

/**
 * @brief Disconnect Wi-Fi cleanly
 */
void wifi_manager_stop(void);

/**
 * @brief Check if Wi-Fi is connected AND valid IP has been acquired
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Read current Wi-Fi status and diagnostic metrics
 */
const WifiManagerStatus* wifi_manager_get_status(void);

/**
 * @brief Periodic tick to process exponential backoff reconnect timers
 */
void wifi_manager_tick(uint32_t current_time_ms);

#if !defined(ESP_PLATFORM)
/**
 * @brief Mock event trigger for Native testing
 * @param event 1=STA_START, 2=STA_CONNECTED, 3=GOT_IP, 4=STA_DISCONNECTED
 * @param mock_ip Optional IP string when event=3
 */
void wifi_manager_mock_trigger_event(int event, const char *mock_ip);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
