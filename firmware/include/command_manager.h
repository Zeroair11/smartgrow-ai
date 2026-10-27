/**
 * @file command_manager.h
 * @brief Downlink Command Processing, Idempotency, TTL Enforcement & Execution
 */

#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "safety_manager.h"

#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CMD_TYPE_UNKNOWN       = 0,
    CMD_TYPE_PUMP_START    = 1,
    CMD_TYPE_PUMP_STOP     = 2,
    CMD_TYPE_VENT_SET      = 3,
    CMD_TYPE_BUZZER_BEEP   = 4,
    CMD_TYPE_RESTART       = 5,
    CMD_TYPE_CONFIG_UPDATE = 6
} CommandType;

typedef struct {
    char        command_id[48];
    char        idempotency_key[48];
    CommandType type;
    uint32_t    param_val;       /* e.g., duration_sec or vent angle */
    uint32_t    expires_at_sec;  /* Unix epoch or relative expiry */
} DeviceCommand;

typedef struct {
    char        command_id[48];
    bool        success;
    const char* status;          /* "completed", "rejected", "failed" */
    int         error_code;
    char        message[64];
} CommandExecutionResult;

/**
 * @brief Initialize command manager and idempotency ring buffer
 */
void command_manager_init(void);

/**
 * @brief Process and execute an incoming command with all safety interlocks
 */
bool command_manager_execute(const DeviceCommand *cmd, const SensorReadings *readings, uint32_t current_epoch_sec, uint32_t current_time_ms, CommandExecutionResult *out_result);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_MANAGER_H */
