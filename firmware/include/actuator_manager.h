/**
 * @file actuator_manager.h
 * @brief Actuator Control (Pump Relay, Ventilation Servo, Buzzer)
 */

#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool    pump_active;
    uint8_t vent_servo_angle_deg; /* 0 (closed) to 90/180 (fully open) */
    bool    buzzer_active;
    bool    led_active;
} ActuatorState;

/**
 * @brief Initialize GPIOs and set all actuators to safe inactive states
 */
void actuator_manager_init(void);

/**
 * @brief Directly set pump state (should only be invoked via Safety Manager)
 */
void actuator_manager_set_pump(bool enable);

/**
 * @brief Set ventilation servo position
 */
void actuator_manager_set_servo(uint8_t angle_deg);

/**
 * @brief Trigger buzzer alert
 */
void actuator_manager_set_buzzer(bool enable);

/**
 * @brief Get current actuator snapshot
 */
const ActuatorState* actuator_manager_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* ACTUATOR_MANAGER_H */
