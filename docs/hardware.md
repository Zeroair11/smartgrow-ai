# SmartGrow AI — Hardware Specification & Wiring Guide

## 1. Primary Controller: ESP32-S3 DevModule N16R8
- **MCU:** Espressif ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz
- **Flash:** 16 MB Quad/Octal SPI Flash
- **PSRAM:** 8 MB Octal SPI PSRAM
- **Wireless:** 2.4 GHz Wi-Fi 802.11 b/g/n + Bluetooth 5 (LE)
- **Power Supply:** 5V DC via USB-C or VIN pin (regulated on-board to 3.3V)

---

## 2. GPIO Pinout & Peripheral Mapping

All pin assignments are centralized in `firmware/include/board_config.h`:

| Peripheral | Component | Pin / Channel | Electrical Type | Configuration Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Soil Moisture** | Capacitive Soil v1.2 / v2.0 | **GPIO 1** | ADC1_CH0 (Analog) | 0 - 3.3V input range; 12-bit ADC (0 - 4095) |
| **Ambient Light** | Photoresistor LDR + 10k divider | **GPIO 2** | ADC1_CH1 (Analog) | Voltage divider to 3.3V |
| **Air Temp & Humidity** | DHT22 / AM2302 Sensor | **GPIO 4** | Digital (One-Wire) | Requires 4.7k - 10k pull-up resistor to 3.3V |
| **Water Pump Relay** | 1-Channel 5V Optocoupled Relay | **GPIO 10** | Digital Output | **Active-Low** by default (`RELAY_PUMP_ACTIVE_LEVEL = 0`) |
| **Ventilation Servo** | SG90 / MG90S Micro Servo | **GPIO 11** | PWM Output | 50 Hz PWM frequency, 1.0ms - 2.0ms pulse width |
| **Audible Alarm** | Active Piezo Buzzer | **GPIO 12** | Digital Output | Active-High drive |
| **Diagnostics LED** | On-board / External LED | **GPIO 21** | Digital Output | Active-High drive |
| **I2C SDA** | Optional SSD1306 OLED (0.96") | **GPIO 8** | I2C Data | 4.7k pull-up to 3.3V |
| **I2C SCL** | Optional SSD1306 OLED (0.96") | **GPIO 9** | I2C Clock | 4.7k pull-up to 3.3V |

---

## 3. Power Distribution & Electrical Protection

> [!CAUTION]
> Never power a water pump or motor directly from the ESP32-S3 3.3V or 5V development board pins! Inductive kickback and motor inrush current will cause CPU brownouts and can permanently damage the controller.

1. **Power Isolation:** Use an external 5V or 12V DC power adapter dedicated to the water pump and servo.
2. **Optocoupler Isolation:** Use an optocoupled relay module. Ensure the relay logic VCC is powered by 3.3V/5V and common ground (GND) is bonded cleanly.
3. **Snubber / Flyback Diode:** For DC inductive pumps, place a 1N4007 flyback diode across the pump motor terminals (cathode to positive, anode to negative) to suppress inductive spikes.

---

## 4. Soil Moisture Sensor Two-Point Calibration Procedure

Capacitive soil moisture sensors output varying analog voltages depending on soil compaction, salinity, and probe tolerance. Calibration maps raw ADC values to 0.0% – 100.0%:

1. **Step 1 (Dry Calibration):**
   - Clean and completely dry the probe blade with a cloth.
   - Suspend the probe in ambient air (0% soil moisture).
   - Read the raw ADC value (typically ~3200 on 12-bit ADC).
   - Record this value as `adc_dry_raw`.
2. **Step 2 (Wet Calibration):**
   - Submerge the probe blade into a cup of tap water up to the marked fill line (do not submerge onboard electronics).
   - Allow reading to settle and record raw ADC value (typically ~1200).
   - Record this value as `adc_wet_raw`.
3. **Step 3 (Configuration):**
   - Update device calibration via the web UI in `ESP32-S3 Devices -> Config Drawer` or via REST API:
     ```json
     {
       "soil_dry_adc": 3200,
       "soil_wet_adc": 1200
     }
     ```
   - The firmware `sensor_calibration_raw_to_moisture_pct()` formula executes on-chip:
     $$\text{Moisture \%} = \frac{\text{Dry ADC} - \text{Raw ADC}}{\text{Dry ADC} - \text{Wet ADC}} \times 100\%$$
