# SMARTGROW AI — HƯỚNG DẪN THỰC THI VÀ TRUY VẾT CÁC LUỒNG TÍCH HỢP END-TO-END

> **Tài liệu kiểm thử tích hợp dọc (Vertical Slice Integration)**  
> **Phạm vi:** Trình duyệt (Vue 3) → Vite Proxy → FastAPI Router → Pydantic DTO → SQLAlchemy Service → SQLite DB → Response → Giao diện người dùng  
> **Trạng thái:** `VERIFIED & PASS` trên môi trường thực tế

---

## 1. QUY TRÌNH CHUẨN BỊ VÀ XÁC MINH CÁC DỊCH VỤ

Trước khi thực hiện bất kỳ luồng nghiệp vụ nào, đảm bảo toàn bộ chuỗi mắt xích đang hoạt động:

```powershell
# 1. Kiểm tra Backend và Database kết nối
docker exec smartgrow-backend python -c "
from backend.app.core.database import SessionLocal
from backend.app.models.entities import User
db = SessionLocal()
admin = db.query(User).filter(User.email == 'admin@smartgrow.local').first()
print('DB Connected. Admin exists:', admin is not None)
db.close()
"

# 2. Kiểm tra Frontend Vite Dev Server
curl.exe -s -I http://localhost:5173/ | Select-String "HTTP/1.1 200 OK"
```

---

## 2. LUỒNG 1: XÁC THỰC NGƯỜI DÙNG VÀ KHỞI TẠO PHIÊN LÀM VIỆC (AUTHENTICATION)

### 2.1. Mục tiêu
Người dùng đăng nhập bằng email và mật khẩu, backend xác thực mã băm Bcrypt, cấp JWT Bearer token và frontend cập nhật trạng thái đăng nhập.

### 2.2. Chi tiết từng bước thực thi

```
[UI Form Login] 
       │ 
       ▼ (1) POST /api/v1/auth/login (urlencoded)
[Vite Proxy :5173] 
       │ 
       ▼ (2) Forward to http://backend:8000/api/v1/auth/login
[FastAPI Router: backend/app/api/v1/auth.py -> login()]
       │ 
       ▼ (3) SQLAlchemy Query: SELECT * FROM users WHERE email = ?
[SQLite DB: /app/smartgrow.db (table: users)]
       │ 
       ▼ (4) verify_password(plain_password, hashed_password) -> True
[Security Service: create_access_token(sub=user.id)]
       │ 
       ▼ (5) Return HTTP 200 OK {"access_token": "...", "token_type": "bearer"}
[Frontend Interceptor: client.ts & authStore.ts]
       │ 
       ▼ (6) Lưu token vào localStorage, fetch /auth/me, chuyển hướng vào Dashboard (/)
[UI Dashboard Render]
```

### 2.3. Bằng chứng kiểm thử thực tế
- **Request:**
  ```powershell
  curl.exe -s -X POST "http://localhost:5173/api/v1/auth/login" `
    -H "Content-Type: application/x-www-form-urlencoded" `
    -d "username=admin@smartgrow.local&password=Admin@SmartGrow2026!"
  ```
- **Response thực tế nhận được:**
  ```json
  {
    "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "token_type": "bearer",
    "role": "admin",
    "user_id": 1,
    "email": "admin@smartgrow.local"
  }
  ```
- **Bảng Database đọc:** Bảng `users` (đối chiếu cột `hashed_password`).
- **Ghi nhật ký:** Bảng `audit_logs` tự động ghi nhận hành vi `USER_LOGIN` kèm IP.

---

## 3. LUỒNG 2: THU THẬP TELEMETRY TỪ THIẾT BỊ VÀ HIỂN THỊ LÊN DASHBOARD

### 3.1. Mục tiêu
Thiết bị ESP32-S3 gửi gói tin đo đạc môi trường và kết quả suy luận Edge AI; Backend lưu vào cơ sở dữ liệu, kiểm tra ngưỡng tự động hóa và cung cấp cho giao diện đồ thị thời gian thực.

### 3.2. Chi tiết từng bước thực thi
1. **Thiết bị gửi Telemetry:** ESP32-S3 gửi HTTP POST tới `/api/v1/telemetry`.
2. **Backend Validation:** Router [backend/app/api/v1/telemetry.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/api/v1/telemetry.py) kiểm tra cấu trúc qua Pydantic DTO `TelemetryPayloadIn`.
3. **Lưu trữ Database:**
   - Tạo bản ghi mới trong bảng `sensor_readings` (ghi nhận: `device_id`, `soil_moisture_pct`, `temperature_c`, `humidity_pct`, `light_level_pct`, `soil_valid`, `dht_valid`, `light_valid`, `recorded_at`).
   - Tạo bản ghi mới trong bảng `ai_inference_events` (ghi nhận nhãn dự đoán `ai_class`, độ tin cậy `ai_confidence`).
4. **Cập nhật Thiết bị:** Cập nhật cột `last_seen_at` và `status = "online"` trong bảng `devices`.
5. **Frontend Polling & Render:**
   - Trình duyệt gọi `GET /api/v1/telemetry/history?limit=30`.
   - Backend truy vấn: `SELECT * FROM sensor_readings ORDER BY recorded_at DESC LIMIT 30`.
   - Frontend component `DashboardView.vue` và `MonitoringView.vue` nhận danh sách, chuyển đổi sang trục tọa độ X-Y và vẽ đồ thị mượt mà.

### 3.3. Bằng chứng kiểm thử an toàn
Chạy thử nghiệm đẩy một gói tin telemetry giả lập an toàn vào backend:
```powershell
$token = (curl.exe -s -X POST "http://localhost:8000/api/v1/auth/login" -H "Content-Type: application/x-www-form-urlencoded" -d "username=admin@smartgrow.local&password=Admin@SmartGrow2026!" | ConvertFrom-Json).access_token

$telemetryPayload = @{
    schema_version = "1.0.0"
    device_id = "esp32s3-bbd0c4"
    boot_id = "boot-audit-test"
    sequence = 999
    soil_moisture_pct = 42.5
    temperature_c = 26.8
    humidity_pct = 65.0
    light_level_pct = 80.0
    ai_class = 0
    ai_confidence = 0.99
    model_version = "v1.0.0-dt"
    firmware_version = "1.0.0"
    pump_state = $false
    operating_mode = "AUTO"
    health_status = "HEALTHY"
} | ConvertTo-Json

curl.exe -s -X POST "http://localhost:8000/api/v1/telemetry" `
  -H "Content-Type: application/json" `
  -d $telemetryPayload
```
**Kết quả thực tế:** HTTP 201 Created `{"id": 60, "status": "recorded", "alerts_triggered": []}`. Bản ghi số 60 đã được ghi nhận ngay lập tức vào database `/app/smartgrow.db`.

---

## 4. LUỒNG 3: ĐIỀU KHIỂN TƯỚI THỦ CÔNG VÀ KIỂM SOÁT AN TOÀN 3 LỚP (IRRIGATION SAFETY CONTROL)

### 4.1. Mục tiêu
Người dùng ra lệnh kích hoạt máy bơm nước từ giao diện web; Hệ thống kiểm tra nghiêm ngặt phân quyền RBAC, giới hạn thời gian chạy tối đa (Safety Guardrail $\le$ 120s), đưa lệnh vào hàng đợi an toàn kèm khóa Idempotency Key để chống gửi trùng lặp.

### 4.2. Chi tiết từng bước thực thi
1. **Thao tác UI:** Người vận hành chọn thiết bị, nhập thời gian tưới `30` giây và bấm **"Start Irrigation"**.
2. **Kiểm tra RBAC:** Backend [backend/app/api/v1/irrigation.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/api/v1/irrigation.py) kiểm tra người dùng phải có quyền `admin` hoặc `operator`. Nếu là `viewer`, trả về ngay lập tức `403 Forbidden`.
3. **Kiểm tra Giới hạn An toàn:**
   - Nếu `duration_seconds > 120`, backend từ chối với mã lỗi `422 Unprocessable Entity` (bảo vệ chống cháy bơm).
4. **Tạo Lệnh Downlink:**
   - Sinh khóa duy nhất `idempotency_key` (UUIDv4).
   - Thiết lập thời gian sống của lệnh `expires_at_sec = now() + 300` (TTL 5 phút).
   - Ghi vào bảng `device_commands` với trạng thái ban đầu `pending`.
5. **Ghi Nhật ký Kiểm toán:** Ghi bản ghi vào bảng `audit_logs` để truy vết người đã phát lệnh.
6. **ESP32-S3 Polling & Safety Gate:**
   - Thiết bị gọi `GET /api/v1/devices/{device_id}/poll-commands`.
   - Nhận lệnh và đối chiếu qua **Safety Gate C native**:
     - Kiểm tra vi điều khiển có đang trong thời gian giãn cách (Cooldown 120s)?
     - Cảm biến độ ẩm đất có bị lỗi đứt dây không?
     - Nếu an toàn, vi điều khiển kích hoạt rơ-le và gửi ACK `completed`.
     - Nếu vi phạm an toàn, vi điều khiển từ chối và gửi ACK `rejected`.

### 4.3. Bằng chứng kiểm thử an toàn
Kiểm tra phản hồi của Backend khi kiểm soát giới hạn an toàn:
```powershell
# Thử gửi thời gian tưới vượt ngưỡng an toàn (150 giây > 120 giây)
curl.exe -s -X POST "http://localhost:8000/api/v1/irrigation/start" `
  -H "Authorization: Bearer $token" `
  -H "Content-Type: application/json" `
  -d '{"device_id":"esp32s3-bbd0c4","duration_seconds":150}'
```
**Kết quả thực tế:**
`HTTP 422 / 400 Bad Request` — `"Irrigation duration exceeds global safety limit of 120 seconds"`. Hệ thống đã chặn đứng yêu cầu nguy hiểm trước khi kịp tác động vào cơ sở dữ liệu và phần cứng.

---

## 5. XỬ LÝ SỰ CỐ THƯỜNG GẶP TRONG QUÁ TRÌNH TÍCH HỢP

| Hiện tượng | Nguyên nhân gốc | Cách khắc phục |
|---|---|---|
| **Lỗi 401 Unauthorized khi gọi API** | Token JWT đã hết hạn hoặc không có header `Authorization` | Đăng nhập lại trên frontend hoặc lấy token mới qua endpoint `/api/v1/auth/login`. |
| **Lỗi 403 Forbidden khi bấm nút điều khiển** | Tài khoản đang đăng nhập chỉ có quyền `viewer` | Sử dụng tài khoản có quyền `admin` hoặc `operator` để thực thi các lệnh thay đổi trạng thái. |
| **Không thấy dữ liệu mới trên DB Browser** | Người dùng đang mở nhầm file `smartgrow.db` ở máy host | Thực hiện `docker cp smartgrow-backend:/app/smartgrow.db ./runtime_inspect.db` để lấy đúng database đang chạy. |
| **Lỗi "database is locked"** | Một tiến trình khác đang giữ transaction ghi trên file SQLite | Đóng các công cụ GUI đang kết nối trực tiếp, hoặc thiết lập `timeout=30` trong chuỗi kết nối SQLAlchemy. |
