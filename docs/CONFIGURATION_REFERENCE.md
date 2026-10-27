# Tài Liệu Tra Cứu Cấu Hình Toàn Diện Hệ Thống (Master Configuration Reference)

**Document Version:** 2.0.0  
**Tài liệu hướng dẫn chi tiết:** [`docs/FIRMWARE_CONFIGURATION_GUIDE.md`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FIRMWARE_CONFIGURATION_GUIDE.md)  
**File cấu hình trung tâm compile-time:** [`firmware/include/project_config.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h)

---

## 1. Nguyên Tắc Quản Trị Cấu Hình & Thứ Tự Ưu Tiên

Hệ thống phân chia cấu hình thành 4 tầng quản lý rõ ràng:
1. **Compile-time Defaults**: Được quản lý tập trung tại [`firmware/include/project_config.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h). Đây là giá trị cơ sở xuất xưởng.
2. **Developer Lab Overrides**: Tệp cục bộ `firmware/include/config_local.h` (được `.gitignore` bảo vệ, không bao giờ commit secret lên Git).
3. **NVS Persistence**: Lưu trong phân vùng Flash NVS của ESP32. Cho phép cập nhật động qua lệnh `CONFIG_UPDATE` và duy trì qua các lần reboot.
4. **Backend Cloud Updates**: Cấu hình cập nhật từ xa thông qua REST API downlink.

**Thứ tự ưu tiên:**
$$\text{Compile-time Defaults} \longrightarrow \text{config\_local.h} \longrightarrow \text{NVS Persistence} \longrightarrow \text{Backend Cloud Update}$$

---

## 2. Bảng Tham Chiếu Toàn Bộ Thông Số Kỹ Thuật & Cấu Hình

| Khóa cấu hình / Symbol | Giá trị mặc định | Đơn vị | Nguồn cấu hình | Tệp và Dòng định nghĩa | Nơi sử dụng | Cần Rebuild / Restart? | Mức độ rủi ro |
|---|---|---|---|---|---|---|---|
| `PROJECT_SAFETY_MAX_PUMP_DURATION_MS` | `120000` (120s) | ms | Compile-time Macro | `firmware/include/project_config.h:112` | `safety_manager.c`, `command_manager.c` | Cần Rebuild | **RẤT CAO**: Tăng giá trị có thể gây cháy bơm hoặc ngập úng |
| `PROJECT_SAFETY_MIN_PUMP_COOLDOWN_MS` | `180000` (180s) | ms | Compile-time Macro | `firmware/include/project_config.h:116` | `safety_manager.c` | Cần Rebuild | **CAO**: Giảm giá trị có thể làm hỏng động cơ bơm do bật tắt liên tục |
| `PROJECT_SAFETY_OFFLINE_FAILSAFE_MS` | `300000` (300s) | ms | Compile-time Macro | `firmware/include/project_config.h:120` | `safety_manager.c` | Cần Rebuild | **TRUNG BÌNH**: Thời gian mất kết nối trước khi ngắt an toàn |
| `PROJECT_PIN_RELAY_PUMP` | `10` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:89` | `actuator_manager.c` | Cần Rebuild | **RẤT CAO**: **CẤM GPIO 15-18 & 33-37** (Octal PSRAM / Flash) |
| `PROJECT_RELAY_PUMP_ACTIVE_LEVEL` | `1` (Active-HIGH) | 0/1 | Compile-time Macro | `firmware/include/project_config.h:108` | `actuator_manager.c` | Cần Rebuild | **RẤT CAO**: Cấu hình sai cực tính làm bơm đóng ngược |
| `PROJECT_PIN_SERVO_VENT` | `11` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:93` | `actuator_manager.c` | Cần Rebuild | **TRUNG BÌNH**: Điều khiển cửa thông gió |
| `PROJECT_PIN_BUZZER` | `12` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:97` | `actuator_manager.c` | Cần Rebuild | **THẤP**: Còi báo động sự cố |
| `PROJECT_PIN_STATUS_LED` | `21` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:101` | `actuator_manager.c` | Cần Rebuild | **THẤP**: Đèn LED chẩn đoán |
| `PROJECT_PIN_DHT11` | `4` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:75` | `dht11.c`, `sensor_manager.c` | Cần Rebuild | **CAO**: Cảm biến nhiệt ẩm không khí |
| `PROJECT_PIN_SOIL_ADC` | `1` (ADC1_CH0) | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:65` | `sensor_manager.c` | Cần Rebuild | **CAO**: Cảm biến đo độ ẩm đất điện dung |
| `PROJECT_PIN_LDR_ADC` | `2` (ADC1_CH1) | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:69` | `sensor_manager.c` | Cần Rebuild | **TRUNG BÌNH**: Cảm biến quang trở ánh sáng |
| `PROJECT_PIN_I2C_SDA` | `8` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:79` | `firmware_config.c` | Cần Rebuild | **THẤP**: I2C Data bus |
| `PROJECT_PIN_I2C_SCL` | `9` | GPIO Pin | Compile-time Macro | `firmware/include/project_config.h:83` | `firmware_config.c` | Cần Rebuild | **THẤP**: I2C Clock bus |
| `PROJECT_DHT11_SAMPLE_INTERVAL_MS` | `2000` | ms | Compile-time Macro | `firmware/include/project_config.h:134` | `dht11.c`, `main.c` | Cần Rebuild | **TRUNG BÌNH**: Chu kỳ đọc cảm biến DHT11 (>= 1000ms) |
| `PROJECT_SOIL_ADC_DRY_RAW` | `3200` | Counts | NVS / Macro | `firmware/include/project_config.h:154` | `sensor_calibration.c` | Hot-update NVS | **TRUNG BÌNH**: Điểm hiệu chuẩn đất khô trong không khí |
| `PROJECT_SOIL_ADC_WET_RAW` | `1200` | Counts | NVS / Macro | `firmware/include/project_config.h:158` | `sensor_calibration.c` | Hot-update NVS | **TRUNG BÌNH**: Điểm hiệu chuẩn đất ướt trong nước |
| `PROJECT_TELEMETRY_INTERVAL_MS` | `5000` | ms | Compile-time Macro | `firmware/include/project_config.h:234` | `main.c` | Cần Rebuild | **TRUNG BÌNH**: Chu kỳ gửi Telemetry lên Cloud |
| `PROJECT_COMMAND_POLL_INTERVAL_MS`| `3000` | ms | Compile-time Macro | `firmware/include/project_config.h:238` | `main.c` | Cần Rebuild | **TRUNG BÌNH**: Chu kỳ poll lệnh điều khiển |
| `PROJECT_CONTROL_MODE_DEFAULT` | `0` (AUTO) | Enum | NVS / Macro | `firmware/include/project_config.h:176` | `main.c`, `firmware_config.c` | Hot-update NVS | **CAO**: Chế độ điều khiển chính của trạm |
| `PROJECT_IDEMPOTENCY_HISTORY_SIZE`| `8` | Slots | Compile-time Macro | `firmware/include/project_config.h:258` | `command_manager.c` | Cần Rebuild | **TRUNG BÌNH**: Bộ nhớ ring buffer lọc lệnh trùng lặp |
| `SECRET_KEY` | *(Ngẫu nhiên)* | String | Env Var | `backend/app/core/config.py:12` | `security.py`, `auth_service.py` | Restart Backend | **TỐI MẬT**: Khóa mã hóa JWT Token |
| `ACCESS_TOKEN_EXPIRE_MINUTES` | `480` (8h) | Phút | Env Var | `backend/app/core/config.py:13` | `auth_service.py` | Restart Backend | **THẤP**: Thời gian sống của phiên đăng nhập |
| `SQLALCHEMY_DATABASE_URI` | `sqlite:///./smartgrow.db` | URI | Env Var | `backend/app/core/config.py:14` | `database.py` | Restart Backend | **CAO**: Đường dẫn cơ sở dữ liệu |
| `DEVICE_REGISTRATION_TOKEN` | *(Bí mật)* | String | Env Var | `backend/app/core/config.py:15` | `devices.py` | Restart Backend | **CAO**: Token đăng ký thiết bị IoT |

---

## 3. Quy Trình Thay Đổi Cấu Hình An Toàn

Xem hướng dẫn đầy đủ và các kịch bản mẫu tại:  
👉 [`docs/FIRMWARE_CONFIGURATION_GUIDE.md`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FIRMWARE_CONFIGURATION_GUIDE.md)
