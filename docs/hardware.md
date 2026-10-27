# SmartGrow AI — Hardware Specification & Wiring Guide

> [!NOTE]
> **TÀI LIỆU THAM CHIẾU PHẦN CỨNG CHÍNH THỨC:**
> Chi tiết toàn bộ linh kiện, hướng dẫn đấu nối, đo kiểm DMM và phân tích cấm chân GPIO 16 được quy định chính thức tại:  
> 👉 [`docs/HARDWARE_WIRING_AND_GPIO.md`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/HARDWARE_WIRING_AND_GPIO.md)

## 1. Primary Controller: ESP32-S3 DevModule N16R8
- **MCU:** Espressif ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz
- **Flash:** 16 MB Quad/Octal SPI Flash
- **PSRAM:** 8 MB Octal SPI PSRAM
- **Wireless:** 2.4 GHz Wi-Fi 802.11 b/g/n + Bluetooth 5 (LE)
- **Power Supply:** 5V DC via USB-C or VIN pin (regulated on-board to 3.3V)

---

## 2. GPIO Pinout & Peripheral Mapping

All hardware pin assignments, electrical characteristics, and sensor boundaries are managed centrally in `firmware/include/project_config.h` (mapped via `firmware_config.h` and `config_defaults.h`):

| Peripheral | Component | Pin / Channel | Electrical Type | Configuration Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Soil Moisture** | Capacitive Soil v1.2 / v2.0 | **GPIO 1** | ADC1_CH0 (Analog) | 0 - 3.3V input range; 12-bit ADC (0 - 4095) |
| **Ambient Light** | Photoresistor LDR + 10k divider | **GPIO 2** | ADC1_CH1 (Analog) | Voltage divider to 3.3V |
| **Air Temp & Humidity** | **DHT11 Sensor** | **GPIO 4** | Digital (Single-Wire) | 3-pin PCB module has built-in pull-up; raw 4-pin sensor requires 4.7k - 10k pull-up to 3.3V |
| **Water Pump Relay** | 1-Channel 5V Optocoupled Relay | **GPIO 10** | Digital Output | Configurable polarity (`relay_pump_active_level`). Default is `1` (Active-High) in `config_defaults.h`; typical optocoupled modules (Songle SRD-05VDC-SL-C) feature High/Low trigger jumper or are Active-Low (`0`). Pre-flight verification with DMM required. |
| **Ventilation Servo** | SG90 / MG90S Micro Servo | **GPIO 11** | PWM Output | 50 Hz PWM frequency, 1.0ms - 2.0ms pulse width |
| **Audible Alarm** | Active Piezo Buzzer | **GPIO 12** | Digital Output | Active-High drive |
| **Diagnostics LED** | On-board / External LED | **GPIO 21** | Digital Output | Active-High drive (`status_led_active_level = 1`) |
| **I2C SDA** | Optional SSD1306 OLED (0.96") | **GPIO 8** | I2C Data | 4.7k pull-up to 3.3V |
| **I2C SCL** | Optional SSD1306 OLED (0.96") | **GPIO 9** | I2C Clock | 4.7k pull-up to 3.3V |

---

## 3. DHT11 Sensor Specifications & Single-Wire Timing

The environmental sensing subsystem utilizes the **DHT11 digital humidity and temperature sensor**:

| Parameter | Specification (DHT11) | Notes & Firmware Enforcement |
| :--- | :--- | :--- |
| **Temperature Range** | 0 °C to 50 °C | Readings outside this range flag `DHT11_ERR_OUT_OF_RANGE` |
| **Temperature Resolution** | 1 °C | Integer precision with fractional byte compatibility |
| **Temperature Accuracy** | ±2 °C | Nominal factory rating across 0–50°C |
| **Humidity Range** | 20% to 90% RH | Readings outside this range flag `DHT11_ERR_OUT_OF_RANGE` |
| **Humidity Resolution** | 1% RH | Standard relative humidity resolution |
| **Humidity Accuracy** | ±5% RH | Nominal factory rating at 25°C |
| **Sampling Interval** | ≥ 1.0 s (2.0 s enforced) | Firmware rate-limiter rejects requests `< 2000 ms` |
| **Host Start Signal** | ≥ 18 ms LOW pulse | Firmware drives line LOW for 19 ms, releases for 30 µs |
| **Data Transmission** | 40-bit single-wire bus | 5 bytes: Hum_int, Hum_dec, Temp_int, Temp_dec, Checksum |
| **Checksum Rule** | `(Byte0 + Byte1 + Byte2 + Byte3) & 0xFF == Byte4` | Verified in driver; mismatch triggers `DHT11_ERR_CHECKSUM` |
| **Wiring & Pull-Up** | 4.7kΩ – 10kΩ resistor to 3.3V | 3-pin breakout modules usually integrate an SMD pull-up |

> [!IMPORTANT]
> **Sensor Plausibility vs Greenhouse Agronomic Thresholds:**
> - The DHT11 physical limits (0–50°C, 20–90% RH) are used by the firmware driver to detect **sensor hardware faults** (open wires, short circuits, failed readouts).
> - High-level greenhouse irrigation thresholds (e.g., soil moisture < 30%) and ventilation thresholds remain distinct business logic rules and are not distorted by the physical sensor boundary checks.

---

## 4. Power Distribution & Electrical Protection

> [!CAUTION]
> Never power a water pump or motor directly from the ESP32-S3 3.3V or 5V development board pins! Inductive kickback and motor inrush current will cause CPU brownouts and can permanently damage the controller.

1. **Power Isolation:** Use an external 5V or 12V DC power adapter dedicated to the water pump and servo.
2. **Optocoupler Isolation:** Use an optocoupled relay module. Ensure the relay logic VCC is powered by 3.3V/5V and common ground (GND) is bonded cleanly.
3. **Snubber / Flyback Diode:** For DC inductive pumps, place a 1N4007 flyback diode across the pump motor terminals (cathode to positive, anode to negative) to suppress inductive spikes.
4. **Boot-Safe Actuator State & Polarity Alignment:**
   - In `actuator_manager_init()`, the firmware pre-charges GPIO 10 to `!pump_active_level` *before* configuring the pin direction to `GPIO_MODE_OUTPUT`. This guarantees that the relay remains completely de-energized regardless of whether `relay_pump_active_level` is set to 0 (Active-Low) or 1 (Active-High).
   - In `config_defaults.h`, `DEFAULT_RELAY_PUMP_ACTIVE_LEVEL` is set to `1` (Active-High). If your physical module is Active-Low (or jumpered to LOW), override this via `config_local.h` or runtime configuration without modifying source code.

---

## 5. Soil Moisture Sensor Two-Point Calibration Procedure

Capacitive soil moisture sensors output varying analog voltages depending on soil compaction, salinity, and probe tolerance. Calibration maps raw ADC values to 0.0% – 100.0%:

1. **Step 1 (Dry Calibration):**
   - Clean and completely dry the probe blade with a cloth.
   - Suspend the probe in ambient air (0% soil moisture).
   - Read the raw ADC value (typically ~3200 on 12-bit ADC).
   - Record this value as `soil_adc_dry_raw`.
2. **Step 2 (Wet Calibration):**
   - Submerge the probe blade into a cup of tap water up to the marked fill line (do not submerge onboard electronics).
   - Allow reading to settle and record raw ADC value (typically ~1200).
   - Record this value as `soil_adc_wet_raw`.
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

---

## 6. Hardware-in-the-Loop (HIL) Pre-Flight Checklist

Before connecting high-power inductive loads (water pumps, solenoid valves) to the ESP32-S3 setup, the operator must execute this HIL pre-flight protocol in a controlled lab environment.

### 6.1 Phase 1 — Physical & Electrical Pre-Checks (Unpowered & Bench Measurement)
- [ ] **GPIO 10 Pin Verification:** Confirm continuity between ESP32-S3 GPIO 10 (Header Pin) and Relay Module Signal (`IN`) pin using a Digital Multimeter (DMM).
- [ ] **Relay Trigger Polarity Verification:**
  - Check relay module trigger jumper (HIGH or LOW).
  - Power ESP32-S3 and measure GPIO 10 voltage at boot:
    - If `relay_pump_active_level = 1` (Active-High): Boot voltage must be **0.00V ± 0.05V**.
    - If `relay_pump_active_level = 0` (Active-Low): Boot voltage must be **3.30V ± 0.05V**.
  - Verify relay status LED is completely **OFF** during ESP32-S3 boot, reset, and watchdog restarts.
- [ ] **Flyback Diode Check:** Verify 1N4007 or Schottky diode polarity across pump terminal connector (Cathode to +12V/5V, Anode to GND).
- [ ] **GND Isolation & Bonding:** Ensure external motor power ground is common with ESP32-S3 ground through a dedicated star point, avoiding ground bounce loops.

### 6.2 Phase 2 — Dummy Load Testing (NO Physical Pump Connected)
Connect an LED with a 1 kΩ series resistor or a 12V 5W automotive test bulb across the relay Normally Open (NO) and Common (COM) terminals.

- [ ] **Boot-Up Transient Test:** Power cycle the ESP32-S3 5 times. Observe dummy load:
  - *Pass:* Zero blinks or flickers during ROM bootloader and FreeRTOS task startup.
- [ ] **SAFE Mode Gate Test:**
  - Set control mode to `SAFE` (`CONTROL_MODE_SAFE`).
  - Artificially ground ADC1_CH0 (GPIO 1) or set soil moisture to 0% (indicating extreme drought).
  - *Pass:* Relay NO contacts stay OPEN. Dummy load remains OFF. Safety trip counter unaffected.
- [ ] **Sensor Disconnection & Out-of-Range Failsafe:**
  - Set mode to `AUTO` or `RULE`.
  - Disconnect DHT11 data pin (GPIO 4) or soil sensor (GPIO 1).
  - *Pass:* Relay immediately de-energizes within ≤ 2000 ms. Log shows `SENSOR_FAULT_FAILSAFE`.
  - *Integrity:* Verify `in_lockout` remains `false` (sensor fault halts pump safely without false emergency lockouts).
- [ ] **Network Loss & Autonomous Offline Operation:**
  - Power off Wi-Fi router or stop backend server.
  - *Pass:* Firmware continues local sensing cycle every 2000 ms, evaluating Local Rules / Edge AI independently.
- [ ] **Hard Watchdog Cutoff & Emergency Lockout:**
  - In `MANUAL` mode, request pump run for 150 seconds (exceeding 120s safety limit).
  - *Pass:* Firmware hard watchdog cuts off relay at exactly `t = 120,000 ms`.
  - *Pass:* System enters `in_lockout = true`. All subsequent pump requests are strictly rejected (`SAFETY_ERR_LOCKOUT_ACTIVE`).
- [ ] **Lockout Immuntiy Verification:**
  - While in Lockout, trigger a sensor fault or send normal stop requests.
  - *Pass:* Lockout status remains `in_lockout = true`. Only explicit `CMD_CLEAR_LOCKOUT` (or authorized backend override) clears the state.
- [ ] **Cooldown Enforcement:**
  - After a normal 10-second pump cycle, immediately request another pump start.
  - *Pass:* Start request rejected with `SAFETY_ERR_COOLDOWN_ACTIVE` until full 180 seconds have elapsed.

### 6.3 Phase 3 — Live Load Testing (Controlled Submerged Pump)
*Only proceed when Phase 1 and Phase 2 PASS 100%.*

- [ ] **Fluid Immersion:** Ensure mini submersible pump is fully submerged in water before power application to prevent dry-running impeller damage.
- [ ] **Current Draw Measurement:** In-line DC ammeter check during motor spin-up (nominal: 0.3A – 0.8A, inrush peak < 1.5A).
- [ ] **Microphonic & EMI Check:** Monitor serial output at 115200 baud for any CPU resets, brownout detections, or bus lockups when the relay clicks.

### 6.4 Strict PASS/FAIL Criteria & Emergency Abort Conditions
| Condition | Severity | Immediate Action |
| :--- | :--- | :--- |
| Relay switches ON during boot / reset | **CRITICAL FAIL** | Abort HIL. Invert `relay_pump_active_level` in `config_local.h`. |
| Relay contacts chatter or rapidly oscillate | **CRITICAL FAIL** | Abort HIL. Check power supply stability and flyback diode. |
| Motor current exceeds 1.5A DC | **CRITICAL FAIL** | Disconnect external power immediately. Check pump for mechanical blockage. |
| Pump fails to shut off at 120s limit | **FATAL FAIL** | Disconnect power. Firmware safety loop halted. |
| Lockout cleared by sensor fault or normal stop | **SAFETY DEFECT** | Firmware revision required. Do not proceed to load testing. |

