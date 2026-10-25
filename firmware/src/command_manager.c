#include "command_manager.h"
#include "actuator_manager.h"
#include "safety_manager.h"
#include <string.h>
#include <stdio.h>

#define IDEMPOTENCY_HISTORY_SIZE 8

static char s_processed_cmd_ids[IDEMPOTENCY_HISTORY_SIZE][48];
static uint8_t s_history_idx = 0;

void command_manager_init(void) {
    memset(s_processed_cmd_ids, 0, sizeof(s_processed_cmd_ids));
    s_history_idx = 0;
}

static bool is_duplicate(const char *cmd_id) {
    if (!cmd_id || cmd_id[0] == '\0') return false;
    for (int i = 0; i < IDEMPOTENCY_HISTORY_SIZE; i++) {
        if (strncmp(s_processed_cmd_ids[i], cmd_id, 48) == 0) {
            return true;
        }
    }
    return false;
}

static void record_cmd(const char *cmd_id) {
    if (!cmd_id) return;
    strncpy(s_processed_cmd_ids[s_history_idx], cmd_id, 47);
    s_processed_cmd_ids[s_history_idx][47] = '\0';
    s_history_idx = (s_history_idx + 1) % IDEMPOTENCY_HISTORY_SIZE;
}

bool command_manager_execute(const DeviceCommand *cmd, uint32_t current_epoch_sec, uint32_t current_time_ms, CommandExecutionResult *out_result) {
    if (!cmd || !out_result) return false;

    memset(out_result, 0, sizeof(*out_result));
    strncpy(out_result->command_id, cmd->command_id, 47);

    /* 1. TTL / Expiry validation */
    if (cmd->expires_at_sec > 0 && current_epoch_sec > cmd->expires_at_sec) {
        out_result->success = false;
        out_result->status = "expired";
        out_result->error_code = 101;
        snprintf(out_result->message, sizeof(out_result->message), "Command expired before receipt");
        return false;
    }

    /* 2. Idempotency validation */
    if (is_duplicate(cmd->command_id)) {
        out_result->success = true;
        out_result->status = "completed";
        out_result->error_code = 0;
        snprintf(out_result->message, sizeof(out_result->message), "Command previously executed (idempotent ACK)");
        return true;
    }

    /* 3. Command execution routing */
    switch (cmd->type) {
        case CMD_TYPE_PUMP_START: {
            SafetyStatus safety_err;
            if (!safety_manager_can_start_pump(NULL, current_time_ms, &safety_err)) {
                out_result->success = false;
                out_result->status = "rejected";
                out_result->error_code = 201;
                snprintf(out_result->message, sizeof(out_result->message), "Safety interlock trip: code %d", (int)safety_err);
                return false;
            }
            safety_manager_record_pump_start(current_time_ms);
            actuator_manager_set_pump(true);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Pump started for %u sec", cmd->param_val);
            record_cmd(cmd->command_id);
            return true;
        }

        case CMD_TYPE_PUMP_STOP: {
            safety_manager_record_pump_stop(current_time_ms);
            actuator_manager_set_pump(false);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Pump stopped");
            record_cmd(cmd->command_id);
            return true;
        }

        case CMD_TYPE_VENT_SET: {
            actuator_manager_set_servo((uint8_t)cmd->param_val);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Vent angle set to %u deg", cmd->param_val);
            record_cmd(cmd->command_id);
            return true;
        }

        case CMD_TYPE_BUZZER_BEEP: {
            actuator_manager_set_buzzer(true);
            out_result->success = true;
            out_result->status = "completed";
            out_result->error_code = 0;
            snprintf(out_result->message, sizeof(out_result->message), "Buzzer triggered");
            record_cmd(cmd->command_id);
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
