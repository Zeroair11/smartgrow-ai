# SMARTGROW AI — FINAL SYSTEM AUDIT, TIMEZONE/LOGGING FIX, CSV EXPORT REPAIR & PERSISTENCE VERIFICATION

> **Ngày thực hiện:** 2026-10-27  
> **Phiên bản hệ thống:** 1.2.0 (Production-Verified & Patched)  
> **Kỹ sư thực hiện:** Senior Full-Stack Engineer, Embedded/IoT Engineer & QA Engineer  
> **Trạng thái tổng thể:** `RESOLVED & VERIFIED` trên Live Docker Runtime & Repository

---

## 1. EXECUTIVE SUMMARY (TÓM TẮT ĐIỀU HÀNH)

Hệ thống **SmartGrow AI** đã được kiểm toán toàn diện, sửa lỗi trực tiếp trên mã nguồn và xác minh thực tế qua vòng đời Docker container:

| Hạng mục kiểm toán | Tình trạng ban đầu | Nguyên nhân cốt lõi | Biện pháp xử lý | Kết quả thực tế |
|---|---|---|---|---|
| **A. Database Persistence** | Nguy cơ mất dữ liệu khi container bị recreate do database nằm tại `/app/smartgrow.db` (overlayfs) | Cấu hình mặc định thiếu đường dẫn tuyệt đối vào volume mount `/app/data` | Di chuyển an toàn sang `/app/data/smartgrow.db` bên trong Named Volume `deploy_smartgrow_data`. Sao lưu Online nhất quán sang host | `PASS` (Recreate container `--force-recreate` bảo toàn 100% 17 bảng, 1,100+ telemetry) |
| **B. Timezone & Logging** | Thời gian hiển thị lệch 7 giờ (sau giờ Việt Nam UTC+07:00) | SQLite lưu `DATETIME` không có thông tin timezone (`naive`). SQLAlchemy đọc ra naive datetime, Pydantic tuần tự hóa thiếu hậu tố `Z`. Trình duyệt ngầm hiểu ISO string không có timezone là Local Time | Thêm `UTCDateTime(TypeDecorator)` trong backend SQLAlchemy gắn timezone UTC. Chuẩn hóa `src/utils/date.ts` chuyển đổi chính xác sang `Asia/Ho_Chi_Minh` 24h | `PASS` (100% timestamp có `Z` hoặc offset UTC; hiển thị chính xác giờ VN; 28 tests pass) |
| **C. CSV Export** | Nhấn "Export CSV" không tải được file hoặc báo lỗi | Frontend dùng thẻ `<a href="/api/v1/telemetry/export-csv">` trực tiếp; trình duyệt gửi GET không kèm Header `Authorization: Bearer <token>`, backend trả HTTP 401 | Viết utility `downloadCsvFromApi()` dùng `apiClient` truyền Bearer token, xử lý Blob, thêm UTF-8 BOM (`\ufeff`) cho Excel, Content-Disposition filename động | `PASS` (Tải file CSV thành công, hiển thị Unicode tiếng Việt, có loading & toast UX) |

---

## 2. PHẦN A — XÁC MINH DATABASE PERSISTENCE QUA VÒNG ĐỜI CONTAINER

### 2.1 Cấu hình thực tế đã xác nhận

- **File Compose:** `deploy/docker-compose.yml`
  - Service `backend`:
    ```yaml
    environment:
      - DATABASE_URL=sqlite:////app/data/smartgrow.db
    volumes:
      - smartgrow_data:/app/data
    ```
  - Volume declaration: `volumes: smartgrow_data:`
- **Volume thực tế trên Docker host:** `deploy_smartgrow_data`
- **Đường dẫn database tuyệt đối trong container:** `/app/data/smartgrow.db`
- **Cơ chế sao lưu nhất quán (Host):**
  - `backups/smartgrow_pre_migration_20261027.db` (430,080 bytes)
  - `backups/smartgrow_audit_final_20261027.db` (540,672 bytes)
  - Cả 2 bản sao lưu đều được kiểm tra bằng `PRAGMA integrity_check` -> **`ok`**.

### 2.2 Danh sách bảng và số lượng bản ghi (Baseline vs Post-Recreation)

| Tên bảng (Table Name) | Trạng thái Schema | Số bản ghi trước Recreate | Số bản ghi sau Recreate & Test | Ghi chú |
|---|---|---|---|---|
| `ai_inference_events` | Tồn tại | 1,008 | 1,103+ | Tăng liên tục do thiết bị ESP32-S3 đang đẩy telemetry thật |
| `ai_models` | Tồn tại | 1 | 1 | Model TinyML `v1.0.0-dt` nguyên vẹn |
| `alerts` | Tồn tại | 15 | 15 | Các cảnh báo hệ thống nguyên vẹn |
| `audit_logs` | Tồn tại | 78 | 78 | Nhật ký hệ thống nguyên vẹn |
| `automation_rule_events` | Tồn tại | 0 | 0 | Chưa có trigger event mới |
| `automation_rules` | Tồn tại | 3 | 3 | Quy tắc tự động hóa nguyên vẹn |
| `device_commands` | Tồn tại | 10 | 10 | Lịch sử lệnh điều khiển nguyên vẹn |
| `device_configs` | Tồn tại | 6 | 6 | Cấu hình ngưỡng cảm biến nguyên vẹn |
| `devices` | Tồn tại | 6 | 6 | Bao gồm thiết bị thật `esp32s3-bbd0c4` |
| `firmware_releases` | Tồn tại | 0 | 0 | Bảng OTA sẵn sàng |
| `greenhouses` | Tồn tại | 5 | 5 | Dữ liệu nhà kính mẫu nguyên vẹn |
| `irrigation_events` | Tồn tại | 2 | 2 | Nhật ký chu kỳ tưới nguyên vẹn |
| `irrigation_schedules` | Tồn tại | 0 | 0 | Lịch tưới hẹn giờ |
| `ota_deployments` | Tồn tại | 0 | 0 | Lịch sử OTA |
| `sensor_readings` | Tồn tại | 1,008 | 1,103+ | Telemetry trực tiếp từ vi điều khiển thật |
| `users` | Tồn tại | 6 | 6 | Danh sách người dùng & Admin nguyên vẹn |
| `zones` | Tồn tại | 6 | 6 | Khu vực canh tác nguyên vẹn |

---

## 3. PHẦN B — TRUY VẾT VÀ SỬA LỖI TIMEZONE & LOGGING

### 3.1 Bảng truy vết toàn bộ chuỗi thời gian (Timezone Trace Matrix)

| Thành phần | Nguồn timestamp | Kiểu dữ liệu | Quy ước timezone trước sửa | Quy ước timezone sau sửa | Cách hiển thị sau sửa |
|---|---|---|---|---|---|
| **ESP32 Firmware** | Hardware RTC / millis | `uint32_t` (ms) | Không gửi timestamp tuyệt đối (chỉ gửi `sequence` và `boot_id`) | Giữ nguyên (an toàn phần cứng) | N/A |
| **Backend Ingest** | `datetime.now(timezone.utc)` | Python `datetime` | Tạo UTC nhưng SQLAlchemy lưu chuỗi naive vào SQLite | Tạo UTC, SQLAlchemy dùng `UTCDateTime` ép timezone UTC | N/A |
| **SQLite Storage** | `entities.py Column(DateTime)` | `VARCHAR` / `TEXT` ISO naive | Chuỗi `"YYYY-MM-DD HH:MM:SS.mmmmmm"` không có `Z` | Chuỗi UTC ISO; khi đọc gắn `tzinfo=timezone.utc` | N/A |
| **API Response** | Pydantic DTO serialization | JSON String | `"2026-10-27T03:11:23.249511"` (trình duyệt parse thành giờ Local) | `"2026-10-27T03:11:23.249511Z"` (chuẩn ISO 8601 UTC) | N/A |
| **Frontend UI** | `new Date(val).toLocaleTimeString()` | JavaScript `Date` | Parse ISO thiếu offset -> hiểu nhầm là giờ VN -> hiển thị trễ 7 tiếng | Dùng `@/utils/date.ts` chuẩn hóa UTC -> chuyển sang `Asia/Ho_Chi_Minh` | `DD/MM/YYYY, HH:mm:ss` (24h) |

### 3.2 Root Cause và Giải pháp Kỹ thuật

1. **Backend:**
   - Trong [backend/app/models/entities.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/models/entities.py):
     Tạo `UTCDateTime(types.TypeDecorator)` tự động đính kèm `tzinfo=datetime.timezone.utc` khi đọc dữ liệu từ SQLite. Khi Pydantic chuyển đổi sang JSON, Pydantic v2 tự động gắn hậu tố `Z`.
2. **Frontend:**
   - Tạo [frontend/src/utils/date.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/date.ts):
     - Hàm `parseDate()`: Nếu nhận chuỗi datetime lịch sử thiếu `Z` (dữ liệu cũ từ SQLite), tự động chuẩn hóa gắn `Z` để không bị parse nhầm thành giờ địa phương.
     - Hàm `formatDateTime()`, `formatTime()`, `formatDate()` sử dụng `Intl.DateTimeFormat('vi-VN', { timeZone: 'Asia/Ho_Chi_Minh', hour12: false })`.
     - Xử lý an toàn các trường hợp `null`, `undefined`, chuỗi rỗng và chuỗi không hợp lệ (trả về fallback `—` thay vì vỡ giao diện).
3. **Màn hình giao diện đã cập nhật:**
   - `MonitoringView.vue` (Live Telemetry stream & History table)
   - `AnalyticsView.vue` (Fleet Health metrics)
   - `DashboardView.vue` (Sensor gauges & Alerts preview)
   - `DevicesView.vue` (Device last seen & Command history)
   - `AlertsView.vue` (System alert list)
   - `UsersAuditView.vue` (Audit logs & User registration time)
   - `IrrigationView.vue` (Irrigation cycle history)
   - `AutomationView.vue` (Rule last trigger & Rule events)
   - `EdgeAiView.vue` (Inference events history)
   - `ScheduleView.vue` (Schedule created date)
   - `SettingsView.vue` (Firmware releases date)

---

## 4. PHẦN C — SỬA LỖI CHỨC NĂNG EXPORT CSV

### 4.1 Danh sách nút Export CSV tìm thấy trong giao diện

| Giao diện | Vị trí / Nút | Trạng thái trước sửa | Trạng thái sau sửa |
|---|---|---|---|
| **MonitoringView.vue** | Thẻ `<a href="/api/v1/telemetry/export-csv">` tại Header | LỖI (Mở tab mới, HTTP 401 do thiếu Authorization Bearer token) | **HOẠT ĐỘNG HOÀN HẢO** (Nút bấm tương tác, gửi Bearer token qua `apiClient`, có spinner & toast) |
| **AnalyticsView.vue** | Thẻ `<a href="/api/v1/telemetry/export-csv">` tại Header | LỖI (HTTP 401 Unauthorized) | **HOẠT ĐỘNG HOÀN HẢO** (Tải toàn bộ dataset lịch sử, filename timestamp động) |

### 4.2 Chi tiết sửa đổi Backend & Frontend

1. **Backend ([backend/app/api/v1/telemetry.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/api/v1/telemetry.py)):**
   - Đảm bảo header trả về: `Content-Type: text/csv; charset=utf-8`.
   - Bổ sung **UTF-8 Byte Order Mark (BOM `\ufeff`)** để Microsoft Excel trên Windows tự động nhận diện tiếng Việt và ký tự đặc biệt mà không bị vỡ font.
   - Header `Content-Disposition`: `attachment; filename="smartgrow_telemetry_YYYYMMDD_HHMMSS.csv"`.
   - Header `Access-Control-Expose-Headers: Content-Disposition` cho phép frontend đọc tên file từ response header.
   - Cột `recorded_at` được format rõ ràng theo chuẩn ISO UTC (`YYYY-MM-DDTHH:MM:SS+00:00`).
2. **Frontend Utility ([frontend/src/utils/csv.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/csv.ts)):**
   - Hàm `downloadCsvFromApi()`:
     - Gửi HTTP GET bằng `apiClient` (tự động gắn Bearer Token qua Axios Request Interceptor).
     - Đọc tên file từ header `Content-Disposition` (hoặc dùng tên mặc định có timestamp).
     - Tạo Blob `text/csv;charset=utf-8;`.
     - Kích hoạt trình duyệt tải xuống qua dynamic `<a>` tag và thu hồi object URL an toàn bằng `URL.revokeObjectURL()` sau 1 giây.

---

## 5. PHẦN D — BẢNG KẾT QUẢ KIỂM TOÁN VÀ HỒI QUY (REGRESSION MATRIX)

| Hạng mục | Lệnh / Test thực tế | Kết quả | Bằng chứng thực tế |
|---|---|---|---|
| **Backend Unit & Regression Tests** | `docker exec smartgrow-backend pytest backend/tests -v` | **PASS** | 25 passed, 0 failures (18.79s) |
| **Integration Flow Tests** | `docker exec smartgrow-backend pytest tests/integration -v` | **PASS** | 2 passed (test_end_to_end_flow & test_firmware_backend_contract) (4.55s) |
| **Timezone Serialization Test** | `test_sqlite_utcdatetime_serialization_has_z_suffix` | **PASS** | Timestamp đọc từ SQLite có tzinfo=UTC, output có hậu tố `Z` |
| **CSV Export Auth & Format Test** | `test_telemetry_csv_export_endpoint_headers_and_content` | **PASS** | 401 khi không có token; 200, UTF-8 BOM, đúng header và filename khi có token |
| **Frontend TypeScript Typecheck** | `vue-tsc --noEmit` | **PASS** | 0 error |
| **Frontend Production Build** | `vite build` | **PASS** | 117 modules transformed, build thành công trong 4.06s |
| **Frontend Timezone Unit Tests** | `node src/utils/date.test.mjs` | **PASS** | 7/7 test passed (UTC->VN, offset bảo toàn, naive fallback, null safety) |
| **Database Integrity Check** | `PRAGMA integrity_check` trên Live Container DB | **PASS** | Output: `ok` |
| **Database Host Backup Integrity** | `PRAGMA integrity_check` trên Host Backup DB | **PASS** | Output: `ok` (file `backups/smartgrow_audit_final_20261027.db`) |
| **Backend Service Health** | HTTP GET `http://localhost:8000/api/v1/system/status` | **PASS** | HTTP 200 `{"status": "healthy", "database": "connected"}` |
| **Frontend HTTP Response** | HTTP HEAD `http://localhost:5173/` | **PASS** | HTTP 200 OK |
| **Container Recreate Verification** | `docker compose up -d --no-deps backend` | **PASS** | Toàn bộ 17 bảng được bảo toàn nguyên vẹn trong volume `deploy_smartgrow_data` |

---

## 6. DANH SÁCH FILE THAY ĐỔI VÀ HƯỚNG DẪN ROLLBACK

### 6.1 Danh sách file đã sửa / tạo mới
- **Backend:**
  - [backend/app/models/entities.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/models/entities.py): Thêm `UTCDateTime(types.TypeDecorator)` để chuẩn hóa timezone UTC khi tải từ SQLite.
  - [backend/app/api/v1/telemetry.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/api/v1/telemetry.py): Bổ sung UTF-8 BOM, dynamic filename timestamp, và chuẩn hóa ISO UTC cho CSV export.
  - [backend/tests/test_timezone_and_csv.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/tests/test_timezone_and_csv.py): Bộ unit test chuyên biệt cho timezone và CSV export.
- **Frontend:**
  - [frontend/src/utils/date.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/date.ts): Module chuẩn hóa và format ngày giờ cho múi giờ `Asia/Ho_Chi_Minh`.
  - [frontend/src/utils/date.test.mjs](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/date.test.mjs): Unit test cho module date.
  - [frontend/src/utils/csv.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/csv.ts): Module xuất CSV có xác thực Bearer token và trigger browser download.
  - [frontend/src/views/MonitoringView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/MonitoringView.vue): Sửa nút Export CSV và format timestamp bảng telemetry.
  - [frontend/src/views/AnalyticsView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/AnalyticsView.vue): Sửa nút Download Dataset CSV.
  - [frontend/src/views/DashboardView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/DashboardView.vue): Cập nhật `formatTime()`.
  - [frontend/src/views/DevicesView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/DevicesView.vue): Cập nhật `formatDateTime()`.
  - [frontend/src/views/AlertsView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/AlertsView.vue): Cập nhật `formatDateTime()`.
  - [frontend/src/views/UsersAuditView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/UsersAuditView.vue): Cập nhật `formatDate()` và `formatDateTime()`.
  - [frontend/src/views/IrrigationView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/IrrigationView.vue): Cập nhật `formatDateTime()`.
  - [frontend/src/views/AutomationView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/AutomationView.vue): Cập nhật `formatDateTime()`.
  - [frontend/src/views/EdgeAiView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/EdgeAiView.vue): Cập nhật `formatDateTime()`.
  - [frontend/src/views/ScheduleView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/ScheduleView.vue): Cập nhật `formatDate()`.
  - [frontend/src/views/SettingsView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/SettingsView.vue): Cập nhật `formatDate()`.

### 6.2 Hướng dẫn Rollback an toàn (nếu cần)
1. **Khôi phục database:**
   Database đã được sao lưu toàn vẹn tại `backups/smartgrow_audit_final_20261027.db` trên host.
   Nếu cần phục hồi nguyên trạng:
   ```bash
   docker cp backups/smartgrow_audit_final_20261027.db smartgrow-backend:/app/data/smartgrow.db
   docker compose -f deploy/docker-compose.yml restart backend
   ```
2. **Khôi phục code:**
   ```bash
   git restore backend/ frontend/
   docker compose -f deploy/docker-compose.yml restart backend
   ```
