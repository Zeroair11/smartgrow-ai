#include "actuator_manager.h"
#include "board_config.h"
#include "firmware_config.h"

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "esp_log.h"
static const char *TAG = "actuator_mgr";
#endif

static ActuatorState s_state = {0};

void actuator_manager_init(void) {
    s_state.pump_active = false;
    s_state.vent_servo_angle_deg = 0;
    s_state.buzzer_active = false;
    s_state.led_active = false;

#if defined(ESP_PLATFORM)
    const FirmwareConfig *cfg = firmware_config_get();
    uint8_t pump_pin = cfg ? cfg->actuators.pin_relay_pump : PIN_RELAY_PUMP;
    uint8_t pump_active_level = cfg ? cfg->actuators.relay_pump_active_level : RELAY_PUMP_ACTIVE_LEVEL;
    uint8_t led_pin = cfg ? cfg->actuators.pin_status_led : PIN_STATUS_LED;
    uint8_t led_active_level = cfg ? cfg->actuators.status_led_active_level : STATUS_LED_ACTIVE_LEVEL;
    uint8_t buzzer_pin = cfg ? cfg->actuators.pin_buzzer : PIN_BUZZER;

    /* 1. Configure Pump Relay Pin with Safe Inactive Default State (Active-Low: set HIGH) */
    gpio_reset_pin((gpio_num_t)pump_pin);
    gpio_set_level((gpio_num_t)pump_pin, !pump_active_level); /* Pre-charge inactive level before output enable */
    gpio_set_direction((gpio_num_t)pump_pin, GPIO_MODE_OUTPUT);

    /* 2. Configure Status LED Pin */
    gpio_reset_pin((gpio_num_t)led_pin);
    gpio_set_level((gpio_num_t)led_pin, !led_active_level);
    gpio_set_direction((gpio_num_t)led_pin, GPIO_MODE_OUTPUT);

    /* 3. Configure Piezo Buzzer Pin */
    gpio_reset_pin((gpio_num_t)buzzer_pin);
    gpio_set_level((gpio_num_t)buzzer_pin, 0);
    gpio_set_direction((gpio_num_t)buzzer_pin, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "Actuator GPIO pins configured: Pump=GPIO%d (Active-Level=%d, Init=OFF), LED=GPIO%d, Buzzer=GPIO%d",
             pump_pin, pump_active_level, led_pin, buzzer_pin);
#endif
}

void actuator_manager_set_pump(bool enable) {
    s_state.pump_active = enable;
#if defined(ESP_PLATFORM)
    const FirmwareConfig *cfg = firmware_config_get();
    uint8_t pump_pin = cfg ? cfg->actuators.pin_relay_pump : PIN_RELAY_PUMP;
    uint8_t pump_active_level = cfg ? cfg->actuators.relay_pump_active_level : RELAY_PUMP_ACTIVE_LEVEL;
    int level = enable ? pump_active_level : (!pump_active_level);
    gpio_set_level((gpio_num_t)pump_pin, level);
    ESP_LOGI(TAG, "Hardware Pump Relay set to %s (GPIO%d = %d)",
             enable ? "ON" : "OFF", pump_pin, level);
#endif
}

void actuator_manager_set_servo(uint8_t angle_deg) {
    if (angle_deg > 180) angle_deg = 180;
    s_state.vent_servo_angle_deg = angle_deg;
#if defined(ESP_PLATFORM)
    const FirmwareConfig *cfg = firmware_config_get();
    uint8_t servo_pin = cfg ? cfg->actuators.pin_servo_vent : PIN_SERVO_VENT;
    ESP_LOGI(TAG, "Vent Servo angle set to %d deg (GPIO%d)", angle_deg, servo_pin);
#endif
}

void actuator_manager_set_buzzer(bool enable) {
    s_state.buzzer_active = enable;
#if defined(ESP_PLATFORM)
    const FirmwareConfig *cfg = firmware_config_get();
    uint8_t buzzer_pin = cfg ? cfg->actuators.pin_buzzer : PIN_BUZZER;
    gpio_set_level((gpio_num_t)buzzer_pin, enable ? 1 : 0);
#endif
}

const ActuatorState* actuator_manager_get_state(void) {
    return &s_state;
}
