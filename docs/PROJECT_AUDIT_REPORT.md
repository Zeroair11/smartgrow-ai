# BÁO CÁO TOÀN DIỆN KIỂM TOÁN VÀ SỬA LỖI HỆ THỐNG
*(SmartGrow AI — Comprehensive Project Audit & Bug Fixing Report)*

---

## 1. PHẠM VI KIỂM TOÁN VÀ PHƯƠNG PHÁP THỰC HIỆN

Cuộc kiểm toán được thực hiện trên toàn bộ repository SmartGrow AI bao gồm:
- **Frontend**: Vue 3, Vite, TypeScript, Pinia, Router, Client API (`frontend/`).
- **Backend**: FastAPI, Pydantic v2, SQLAlchemy 2.0, Authentication, Command Queue (`backend/`).
- **Database**: SQLite 3 (`smartgrow.db`), schema, khóa ngoại, chỉ mục và quan hệ cascade.
- **Firmware**: C99 / ESP-IDF, FreeRTOS tasks, GPIO drivers, Safety Manager, Network Client (`firmware/`).
- **Machine Learning**: Dataset generation, Decision Tree training, C transpilation, Edge AI runtime (`ml/`).
- **Kiểm thử**: Toàn bộ unit tests, regression tests, integration tests, và test giả lập phần cứng native.

### Phương pháp tiếp cận:
1. **Kiểm toán từ mã nguồn thực tế**: Không dựa trên tài liệu mô tả cũ; truy vết từng đường dẫn gọi hàm, hợp đồng API và kiểm tra an toàn phần cứng.
2. **Xác minh qua thực nghiệm**: Chạy lại 100% các bài test và lệnh build, ghi nhận exit code và log thực tế.
3. **Tuân thủ giới hạn an toàn**: Không flash firmware vào chip vật lý, không kích hoạt rơ-le mang tải điện áp cao, không xóa hoặc làm hỏng dữ liệu CSDL.

---

## 2. BẢNG TỔNG HỢP LỖI PHÁT HIỆN VÀ ĐÃ XỬ LÝ

| Cấp độ nghiêm trọng | Số lượng phát hiện | Đã sửa dứt điểm | Tồn đọng |
|---|---|---|---|
| **P0 — Critical** | 0 | 0 | 0 |
| **P1 — High** | 2 | 2 | 0 |
| **P2 — Medium** | 2 | 2 | 0 |
| **P3 — Low / Quality** | 1 | 1 | 0 |
| **Tổng cộng** | **5** | **5** | **0** |

---

## 3. CHI TIẾT TỪNG LỖI VÀ GIẢI PHÁP SỬA CHỮA

### 3.1. [P1-01] Thiếu trường `expires_at_sec` và `param_val` ở tầng ngoài của Schema `CommandRead`
- **Tệp liên quan**: `backend/app/schemas/dtos.py` (Dòng 211-226), `firmware/src/network_client.c` (Dòng 308-320).
- **Hiện tượng**: Khi Backend trả về danh sách lệnh chờ qua endpoint `GET /api/v1/devices/{device_id}/poll-commands`, đối tượng JSON chỉ chứa `expires_at` dạng ISO8601 string, và `param_val` nằm ẩn trong chuỗi `payload_json`. Trong khi đó, Firmware trích xuất trực tiếp số nguyên cấp ngoài: `extract_json_int(obj_buf, "expires_at_sec", &exp_sec)`.
- **Hậu quả**: Giá trị `cmd->expires_at_sec` trên Firmware luôn bằng `0`. Logic kiểm tra TTL cục bộ trên firmware bị bỏ qua, thiết bị phụ thuộc hoàn toàn vào chu kỳ dọn dẹp lệnh hết hạn của máy chủ.
- **Nguyên nhân gốc (Root cause)**: Bất đồng bộ giữa DTO trả về của FastAPI và trình phân tích cú pháp chuỗi JSON nhẹ (micro JSON parser) trên Firmware.
- **Bản sửa**:
  - Bổ sung `@computed_field` cho `expires_at_sec` (chuyển đổi `expires_at` sang Unix epoch integer) và `param_val` / `duration_s` (trích xuất từ `payload_json`) trực tiếp trong Pydantic v2 `CommandRead`.
- **Kiểm thử hồi quy (Regression Test)**: `backend/tests/test_audit_regressions.py::test_command_polling_includes_expires_at_sec_and_param_val`.
- **Kết quả sau sửa**: **PASS**. Firmware nhận được trực tiếp `expires_at_sec` và kiểm tra TTL tại chỗ thành công.

---

### 3.2. [P1-02] Lỗi sập máy chủ 500 khi xuất Telemetry CSV khi có sự cố cảm biến
- **Tệp liên quan**: `backend/app/api/v1/telemetry.py` (Dòng 76-81).
- **Hiện tượng**: Khi cảm biến bị lỗi hoặc mất kết nối (`soil_valid == false` hoặc `dht_valid == false`), các giá trị đo được lưu trữ trong CSDL dưới dạng `NULL` (`None`). Khi người dùng gọi endpoint `GET /api/v1/telemetry/export-csv`, máy chủ thực thi định dạng chuỗi: `f"{r.soil_moisture_pct:.2f}"`, gây ra ngoại lệ:
  `TypeError: unsupported format string passed to NoneType.__format__` và trả về mã lỗi HTTP 500.
- **Nguyên nhân gốc (Root cause)**: Chưa kiểm tra giá trị `None` trước khi áp dụng format specifier số thực.
- **Bản sửa**:
  - Cập nhật biểu thức định dạng có kiểm tra điều kiện:
    `f"{r.soil_moisture_pct:.2f}" if r.soil_moisture_pct is not None else ""` cho toàn bộ các trường đo lường và điểm tin cậy AI.
- **Kiểm thử hồi quy (Regression Test)**: `backend/tests/test_audit_regressions.py::test_telemetry_csv_export_with_null_sensor_readings`.
- **Kết quả sau sửa**: **PASS**. Xuất tệp CSV thành công với các ô trống khi cảm biến bị hỏng mà không gây sập ứng dụng.

---

### 3.3. [P2-01] Lỗi 422 Unprocessable Entity khi hủy gán phân khu (Unassign Zone)
- **Tệp liên quan**: `backend/app/api/v1/devices.py` (Dòng 142), `frontend/src/views/DevicesView.vue` (Dòng 513).
- **Hiện tượng**: Endpoint `assign_device_zone` khai báo tham số `zone_id: Optional[int]` nhưng không có giá trị mặc định, khiến FastAPI coi đây là Query Parameter bắt buộc. Đồng thời, Frontend gửi chuỗi rỗng: `?zone_id=` khi muốn hủy phân khu, dẫn đến lỗi xác thực 422.
- **Nguyên nhân gốc (Root cause)**: Khai báo tham số FastAPI thiếu `Query(None)` và xử lý URL phía Vue chưa tối ưu.
- **Bản sửa**:
  - Tại Backend: Cập nhật chữ ký hàm thành `zone_id: Optional[int] = Query(None)`.
  - Tại Frontend: Kiểm tra nếu `zoneId !== null` thì truyền `?zone_id=${zoneId}`, ngược lại chỉ gọi `PUT /devices/${deviceId}/zone` không kèm tham số.
- **Kiểm thử hồi quy (Regression Test)**: `backend/tests/test_audit_regressions.py::test_assign_device_zone_optional_query_param`.
- **Kết quả sau sửa**: **PASS**. Thiết bị có thể hủy gán phân khu về `None` an toàn.

---

### 3.4. [P2-02] Bất đồng bộ tùy chọn chế độ vận hành (Control Mode Parity) trên Giao diện
- **Tệp liên quan**: `frontend/src/views/DevicesView.vue` (Dòng 272-276).
- **Hiện tượng**: Menu thả xuống cấu hình thiết bị chỉ hiển thị 3 chế độ: `AUTO`, `MANUAL`, `SAFE`. Người vận hành không thể kích hoạt chế độ `RULE` (Quy tắc cục bộ) hoặc `HYBRID` (Lai giữa AI và Quy tắc) từ giao diện Web, mặc dù Backend và Firmware đều hỗ trợ đầy đủ.
- **Bản sửa**: Bổ sung hai thẻ `<option>` cho `RULE` và `HYBRID` vào `DevicesView.vue`.
- **Kết quả sau sửa**: Giao diện đồng bộ 100% với 5 chế độ của Firmware enum. Kiểm tra build Frontend: **PASS**.

---

### 3.5. [P3-01] Cảnh báo lỗi thời Pydantic v1 `.dict()` trong các API tạo thực thể
- **Tệp liên quan**: `backend/app/api/v1/greenhouses.py`, `zones.py`, `irrigation.py`.
- **Hiện tượng**: Sử dụng phương thức `.dict()` cũ của Pydantic v1 gây ra hàng loạt cảnh báo `PydanticDeprecatedSince20` trong nhật ký chạy của hệ thống.
- **Bản sửa**: Chuyển đổi toàn bộ sang phương thức chuẩn `.model_dump()` của Pydantic v2.
- **Kết quả sau sửa**: Loại bỏ triệt để cảnh báo, mã nguồn tuân thủ chuẩn Pydantic v2 hiện đại.

---

## 4. MA TRẬN KẾT QUẢ KIỂM THỬ THỰC TẾ

| Nhóm kiểm thử | Lệnh thực thi chính xác | Thư mục chạy | Exit Code | Trạng thái |
|---|---|---|---|---|
| **Backend Tests** | `.\backend\.venv\Scripts\python.exe -m pytest backend/tests` | `c:\Users\ADMIN\Downloads\smartgrow-ai` | `0` | **PASS** (19/19 passed) |
| **Integration Tests** | `.\backend\.venv\Scripts\python.exe -m pytest tests/integration` | `c:\Users\ADMIN\Downloads\smartgrow-ai` | `0` | **PASS** (2/2 passed) |
| **Frontend Build** | `npm run build` (`vue-tsc --noEmit && vite build`) | `c:\Users\ADMIN\Downloads\smartgrow-ai\frontend` | `0` | **PASS** (115 modules, 4.43s) |
| **Firmware Native Simulation** | `gcc -I firmware/include ... -o test.exe; .\test.exe` | `c:\Users\ADMIN\Downloads\smartgrow-ai` | `0` | **PASS** (263/263 passed) |
| **ML Edge AI Parity** | `gcc ... -o test_parity.exe; .\test_parity.exe` | `c:\Users\ADMIN\Downloads\smartgrow-ai` | `0` | **PASS** (500/500 matches) |
| **ML Training & Eval** | `.\backend\.venv\Scripts\python.exe -m ml.evaluation.evaluate_model` | `c:\Users\ADMIN\Downloads\smartgrow-ai` | `0` | **PASS** (100% clean acc) |
| **Target ESP32 Flashing** | `pio run -t upload` | `firmware/` | N/A | **NOT RUN** (Ràng buộc an toàn) |

---

## 5. ĐÁNH GIÁ MỨC ĐỘ SẴN SÀNG PHÁT HÀNH (RELEASE READINESS EVALUATION)

> [!IMPORTANT]
> **KẾT LUẬN CỦA BAN KIỂM TOÁN**:
> - Hệ thống đạt mức độ **STABLE IN SOFTWARE & SIMULATION** (Ổn định 100% ở tầng phần mềm, logic an toàn và môi trường giả lập).
> - **CHƯA ĐỦ ĐIỀU KIỆN SẴN SÀNG TRIỂN KHAI VẬT LÝ RA CÁNH ĐỒNG (FIELD PRODUCTION NOT READY)** cho đến khi hoàn thành các điều kiện tiên quyết sau:
>   1. Thực hiện quy trình kiểm tra phần cứng HIL Pre-flight trên bàn thí nghiệm với tải giả lập.
>   2. Triển khai chứng chỉ SSL/TLS trên Backend API và cấu hình firmware sử dụng HTTPS.
>   3. Thay đổi toàn bộ các khóa bí mật (`SECRET_KEY`, `DEVICE_REGISTRATION_TOKEN`, mật khẩu admin mặc định).
>   4. Xác thực đặc tính cơ-điện thực tế của mô-đun Rơ-le (cực tính Active-LOW / Active-HIGH) với phần cứng thương mại cụ thể.
