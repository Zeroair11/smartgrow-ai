# SmartGrow AI — Hardware Validation & Electrical Verification Guide

**Version:** 2.0.0  
**Target:** ESP32-S3 DevModule N16R8 (16MB Flash, 8MB Octal PSRAM)  
**Date:** 2026-10-25  

This document provides the hardware engineering specification, GPIO strapping safety analysis, ADC plausibility boundaries, electrical protection guidelines, and bench verification checklist.

---

## 1. Controller & GPIO Strapping Pin Safety Analysis

The primary controller is the **Espressif ESP32-S3-WROOM-1 / DevModule N16R8**.

### GPIO Pin Mapping & Strapping Review:
| Peripheral | Pin | Type | Strapping Note | Electrical Safety Guidance |
| :--- | :--- | :--- | :--- | :--- |
| **Soil Moisture** | **GPIO 1** | ADC1_CH0 (Analog) | None | Input only ($0 - 3.3\text{V}$). ADC1 is functional concurrently with Wi-Fi. |
| **Ambient Light** | **GPIO 2** | ADC1_CH1 (Analog) | None | Voltage divider to $3.3\text{V}$. |
| **Air Temp/Humidity (DHT11)** | **GPIO 4** | Digital (One-Wire) | None | $4.7\text{k}\Omega - 10\text{k}\Omega$ pull-up to $3.3\text{V}$ required (if sensor bare; board modules typically integrate $10\text{k}\Omega$). Single-wire 19ms start pulse; nominal $0-50^\circ\text{C}$ ($\pm 2^\circ\text{C}$), $20-90\%\text{ RH}$ ($\pm 5\%$), rate-limited to $\ge 2000\text{ms}$. |
| **Water Pump Relay** | **GPIO 10** | Digital Output | None | **Active-Low** (`0` = Relay ON, `1` = Relay OFF). Must have external $4.7\text{k}\Omega$ pull-up to $3.3\text{V}$ to prevent boot glitches. |
| **Ventilation Servo** | **GPIO 11** | PWM Output | None | $50\text{Hz}$ PWM ($1.0\text{ms} - 2.0\text{ms}$ pulse width). |
| **Audible Alarm** | **GPIO 12** | Digital Output | None | Active-High drive. On ESP32-S3, GPIO 12 is a regular IO (unlike ESP32 classic MTDI). |
| **Status LED** | **GPIO 21** | Digital Output | None | Active-High drive with $330\Omega$ current-limiting resistor. |
| **I2C SDA** | **GPIO 8** | I2C Data | None | $4.7\text{k}\Omega$ pull-up to $3.3\text{V}$. |
| **I2C SCL** | **GPIO 9** | I2C Clock | None | $4.7\text{k}\Omega$ pull-up to $3.3\text{V}$. |

> [!IMPORTANT]
> **Strapping Pins on ESP32-S3:** Strapping pins on the ESP32-S3 are GPIO 0, GPIO 3, GPIO 45, and GPIO 46. None of these pins are used by the SmartGrow peripheral configuration, ensuring unhindered bootloader and JTAG execution.

---

## 2. Electrical Protection & Power Isolation Architecture

```
[ External 12V/5V DC Supply ] -------------------+
                                                  |
                                                  v
[ ESP32-S3 (3.3V Logic) ] ---> [ Optocoupler ] -> [ Relay Coil / Contacts ] ---> [ Water Pump DC ]
             |                      |                                                  |
          GND_MCU ----------------- GND_RELAY (Bonded)                      [ 1N4007 Diode ]
```

1. **Galvanic Isolation:** The relay module must feature optocoupler isolation. The ESP32-S3 GPIO 10 drives only the optocoupler LED ($< 5\text{mA}$).
2. **Inductive Kickback Protection:** A 1N4007 flyback diode must be connected across the DC water pump terminals (cathode to positive terminal, anode to negative terminal) to clamp inductive spikes when the motor is de-energized.
3. **Common Ground:** While logic and motor power rails are separated, digital ground (`GND`) must be bonded with low impedance to eliminate floating ground reference offsets on analog sensors.
4. **Power Rail Isolation:** Under no circumstances should the water pump or SG90 servo motor draw current from the ESP32 development board's on-board $3.3\text{V}$ LDO regulator.

---

## 3. ADC Sensor Plausibility & Fault Boundaries

Capacitive soil sensors output an inverted analog voltage (higher moisture corresponds to higher capacitance and lower output voltage).

The firmware enforces the following boundaries via `sensor_calibration_is_adc_plausible()`:

```
0            150                          1200                    3200             3850        4095
[ Short Circuit ] [=== Valid Wet Range ===] [=== Valid Dry Range ===] [=== Disconnected ===]
(soil_valid=0)                       (100.0% Moisture)       (0.0% Moisture)        (soil_valid=0)
```

- **Plauisble ADC Range:** $150 \le \text{ADC} \le 3850$
- **Open-Circuit Detection ($\text{ADC} > 3850$):** Occurs when the probe wire is severed or unplugged. The firmware immediately sets `soil_valid = false`, transitions the health status to `FAULT`, and interlocks the pump actuator off.
- **Short-Circuit Detection ($\text{ADC} < 150$):** Occurs if the signal line shorts to ground. The firmware immediately sets `soil_valid = false` and blocks irrigation.

---

## 4. Flash Layout & Dual-Partition OTA Scheme

The ESP32-S3 N16R8 provides $16\text{MB}$ of SPI Flash. The firmware partitions are defined as follows:

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xf000,   0x2000,
phy_init, data, phy,     0x11000,  0x1000,
factory,  app,  factory, 0x20000,  0x200000,
ota_0,    app,  ota_0,   0x220000, 0x300000,
ota_1,    app,  ota_1,   0x520000, 0x300000,
storage,  data, spiffs,  0x820000, 0x7E0000,
```

- **App Partition Size:** $3\text{MB}$ each for `ota_0` and `ota_1`.
- **Typical Firmware Size:** $\approx 850\text{KB} - 1.2\text{MB}$ (well within the $3\text{MB}$ limit).
- **Rollback Mechanism:** The `otadata` partition records the active boot slot and boot counter. If a newly flashed image fails to call `esp_ota_mark_app_valid_cancel_rollback()` within 30 seconds of boot, the bootloader automatically reverts to the previous stable slot.

---

## 5. Host Hardware Probe & Controlled Bench Test Checklist

### 5.1. Host Detection & Toolchain Verification Baseline
Probing host system during audit:
- `COM8` — Standard Serial over Bluetooth link (Ignored)
- `COM9` — Standard Serial over Bluetooth link (Ignored)
- `COM5` — **USB-Enhanced-SERIAL CH343 (COM5)**:
  - **Silicon Identity:** ESP32-S3 (QFN56, silicon rev v0.2, 40MHz crystal)
  - **Embedded Memory:** 8MB Embedded Octal PSRAM (AP_3v3)
  - **Flash Storage:** 16MB SPI Flash (Quad SPI, 3.3V)
  - **Hardware MAC Address:** `b8:1f:3f:bb:d0:c4`
- **Toolchain Status:** PlatformIO Core 6.2.0 + ESP-IDF 6.1.0 (`toolchain-xtensa-esp-elf @ 15.2.0`).
- **Target Cross-Compilation:** `BUILD_VERIFIED` (`firmware.bin`: 924,637 bytes, `bootloader.bin`: 21,120 bytes, `partitions.bin`: 3,072 bytes).
- **RAM Usage:** 12.6% (41,276 / 327,680 bytes).
- **Flash Usage:** 29.4% (924,637 / 3,145,728 bytes).

### 5.2. Controlled Flashing Instructions (For ESP32-S3 on COM5)
To flash the built firmware to the connected physical ESP32-S3 target:

**Option A — PlatformIO Upload:**
```powershell
& "C:\Users\ADMIN\.platformio\penv\Scripts\pio.exe" run -e esp32s3_n16r8 -t upload --upload-port COM5
```

**Option B — Direct esptool.py Flash:**
```powershell
& "C:\Users\ADMIN\.platformio\penv\Scripts\python.exe" "C:\Users\ADMIN\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM5 --baud 921600 `
    --before default_reset --after hard_reset `
    write_flash -z `
    0x0 "firmware\.pio\build\esp32s3_n16r8\bootloader.bin" `
    0x8000 "firmware\.pio\build\esp32s3_n16r8\partitions.bin" `
    0x10000 "firmware\.pio\build\esp32s3_n16r8\firmware.bin"
```

### 5.3. Controlled Bench Test Procedure
Before connecting live irrigation plumbing or mains power:
1. Verify device COM port connection (`COM5`).
2. Flash firmware image using Option A or B above.
3. Open serial monitor at 115200 baud:
   ```powershell
   & "C:\Users\ADMIN\.platformio\penv\Scripts\pio.exe" device monitor -p COM5 -b 115200
   ```
4. Verify boot banner:
   `SMARTGROW AI — ESP32-S3 EMBEDDED FIRMWARE | Board: ESP32-S3-N16R8 | Version: 1.0.0`
5. Verify Wi-Fi state sequence: `CONNECTING -> CONNECTED_WAITING_IP -> GOT_IP`.
6. Start backend FastAPI service (`uvicorn backend.app.main:app --port 8000`) and verify device auto-enrollment in backend database.
7. Observe periodic telemetry in backend (`/api/v1/telemetry/latest`).
8. Dispatch test command from dashboard or API (`POST /api/v1/devices/{id}/commands`) and verify immediate ACK dispatch.
9. Perform Dry/Wet calibration with capacitive probe in ambient air and water cup.
10. Verify relay click sound without motor load. Verify GPIO 10 active-low behavior with digital multimeter ($3.3\text{V}$ idle, $0\text{V}$ during pump run).
11. Verify pump stops automatically after 120 seconds in continuous run test.
