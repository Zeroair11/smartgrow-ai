#include "command_manager.h"
#include "actuator_manager.h"
#include "safety_manager.h"
#include "project_config.h"
#include <string.h>
#include <stdio.h>

#ifndef IDEMPOTENCY_HISTORY_SIZE
#define IDEMPOTENCY_HISTORY_SIZE PROJECT_IDEMPOTENCY_HISTORY_SIZE
#endif

typedef struct {
    char cmd_id[48];
    char idemp_key[48];
} ProcessedCommand;

static ProcessedCommand s_processed_cmds[IDEMPOTENCY_HISTORY_SIZE];
static uint8_t s_history_idx = 0;

void command_manager_init(void) {
    memset(s_processed_cmds, 0, sizeof(s_processed_cmds));
    s_history_idx = 0;
}

static bool is_duplicate(const char *cmd_id, const char *idemp_key) {
    for (int i = 0; i < IDEMPOTENCY_HISTORY_SIZE; i++) {
        if (cmd_id && cmd_id[0] != '\0' && strncmp(s_processed_cmds[i].cmd_id, cmd_id, 48) == 0) {
            return true;
        }
        if (idemp_key && idemp_key[0] != '\0' && strncmp(s_processed_cmds[i].idemp_key, idemp_key, 48) == 0) {
            return true;
        }
    }
    return false;
}

static void record_cmd(const char *cmd_id, const char *idemp_key) {
    if (cmd_id) {
        strncpy(s_processed_cmds[s_history_idx].cmd_id, cmd_id, 47);
        s_processed_cmds[s_history_idx].cmd_id[47] = '\0';
    } else {
        s_processed_cmds[s_history_idx].cmd_id[0] = '\0';
    }
    if (idemp_key) {
        strncpy(s_processed_cmds[s_history_idx].idemp_key, idemp_key, 47);
        s_processed_cmds[s_history_idx].idemp_key[47] = '\0';
    } else {
        s_processed_cmds[s_history_idx].idemp_key[0] = '\0';
    }
    s_history_idx = (s_history_idx + 1) % IDEMPOTENCY_HISTORY_SIZE;
}

#include "board_config.h"

bool command_manager_execute(const DeviceCommand *cmd, const SensorReadings *readings, uint32_t current_epoch_sec, uint32_t current_time_ms, CommandExecutionResult *out_result) {
    if (!cmd || !out_result) return false;

    memset(out_result, 0, sizeof(*out_result));
    strncpy(out_result->command_id, cmd->command_id, 47);

    /* 1. TTL / Expiry validation (only when current_epoch_sec is a valid Unix epoch > 0) */
    if (cmd->expires_at_sec > 0 && current_epoch_sec > 0 && current_epoch_sec > cmd->expires_at_sec) {
        out_result->success = false;
        out_result->status = "expired";
        out_result->error_code = 101;
        snprintf(out_result->message, sizeof(out_result->message), "Command expired before receipt");
        return false;
    }

    /* 2. Idempotency validation: check both command_id and idempotency_key */
    if (is_duplicate(cmd->command_id, cmd->idempotency_key)) {
        out_result->success = true;
        out_result->status = "completed";
        out_result->error_code = 0;
        snprintf(out_result->message, sizeof(out_result->message), "Command previously executed (idempotent ACK)");
        return true;
    }

    /* 3. Command execution routing */
    switch (cmd->type) {
        case CMD_TYPE_PUMP_START: {
            uint32_t req_duration_s = cmd->param_val;
            if (req_duration_s == 0) req_duration_s = 30;

            /* Hardware limit check: maximum 120 seconds duration */
            if (req_duration_s > (SAFETY_MAX_PUMP_DURATION_MS / 1000)) {
                out_result->success = false;
                out_result->status = "rejected";
                out_result->error_code = 202;
                snprintf(out_result->message, sizeof(out_result->message), "Requested duration %u s exceeds limit 120s", (unsigned)req_duration_s);
                return false;
            }

            SafetyStatus safety_err = SAFETY_OK;
            bool ok = safety_manager_request_pump_on(SAFETY_SRC_DOWNLINK_CMD, req_duration_s * 1000, readings, current_time_ms, &safety_err);
            if (!ok) {
                out_result->success = false;
                out_result->status = "rejected";
                out_result->error_code = 201;
                snprintf(out_result->message, sizeof(out_result->message), "Safety interlock trip: code %d", (int)safety_err);
                return false;
            }
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Pump started for %u sec", (unsigned)req_duration_s);
            record_cmd(cmd->command_id, cmd->idempotency_key);
            return true;
        }

        case CMD_TYPE_PUMP_STOP: {
            safety_manager_request_pump_off(SAFETY_SRC_DOWNLINK_CMD, "MANUAL_COMMAND_STOP", current_time_ms);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Pump stopped");
            record_cmd(cmd->command_id, cmd->idempotency_key);
            return true;
        }

        case CMD_TYPE_VENT_SET: {
            actuator_manager_set_servo((uint8_t)cmd->param_val);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Vent angle set to %u deg", (unsigned)cmd->param_val);
            record_cmd(cmd->command_id, cmd->idempotency_key);
            return true;
        }

        case CMD_TYPE_BUZZER_BEEP: {
            actuator_manager_set_buzzer(true);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Buzzer triggered");
            record_cmd(cmd->command_id, cmd->idempotency_key);
            return true;
        }

        case CMD_TYPE_RESTART: {
            safety_manager_request_pump_off(SAFETY_SRC_DOWNLINK_CMD, "REBOOT_COMMAND", current_time_ms);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Reboot command accepted and queued");
            record_cmd(cmd->command_id, cmd->idempotency_key);
            return true;
        }

        case CMD_TYPE_CONFIG_UPDATE: {
            uint32_t target_mode = cmd->param_val;
            if (target_mode > (uint32_t)CONTROL_MODE_SAFE) {
                out_result->success = false;
                out_result->status = "rejected";
                out_result->error_code = 422;
                snprintf(out_result->message, sizeof(out_result->message), "Invalid operating mode %u (valid: 0-4)", (unsigned)target_mode);
                return false;
            }

            bool ok = firmware_config_set_control_mode((ControlMode)target_mode);
            if (!ok) {
                out_result->success = false;
                out_result->status = "rejected";
                out_result->error_code = 500;
                snprintf(out_result->message, sizeof(out_result->message), "Failed to apply or persist operating mode %u to NVS", (unsigned)target_mode);
                return false;
            }

            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "ControlMode updated to %u and saved to NVS", (unsigned)target_mode);
            record_cmd(cmd->command_id, cmd->idempotency_key);
            return true;
        }

        default:
            out_result->success = false;
            out_result->status = "rejected";
            out_result->error_code = 400;
            snprintf(out_result->message, sizeof(out_result->message), "Unknown command type");
            return false;
    }
}
