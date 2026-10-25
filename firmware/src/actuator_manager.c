#include "actuator_manager.h"
#include "board_config.h"

static ActuatorState s_state = {0};

void actuator_manager_init(void) {
    s_state.pump_active = false;
    s_state.vent_servo_angle_deg = 0;
    s_state.buzzer_active = false;
    s_state.led_active = false;

#if defined(ESP_PLATFORM)
    // Configure GPIO pins as outputs with safe default state
#endif
}

void actuator_manager_set_pump(bool enable) {
    s_state.pump_active = enable;
#if defined(ESP_PLATFORM)
    // GPIO write according to RELAY_PUMP_ACTIVE_LEVEL
#endif
}

void actuator_manager_set_servo(uint8_t angle_deg) {
    if (angle_deg > 180) angle_deg = 180;
    s_state.vent_servo_angle_deg = angle_deg;
#if defined(ESP_PLATFORM)
    // LEDC PWM pulse width control for SG90 servo
#endif
}

void actuator_manager_set_buzzer(bool enable) {
    s_state.buzzer_active = enable;
#if defined(ESP_PLATFORM)
    // GPIO write buzzer
#endif
}

const ActuatorState* actuator_manager_get_state(void) {
    return &s_state;
}
