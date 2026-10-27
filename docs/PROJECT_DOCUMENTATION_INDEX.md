# SMARTGROW AI — MỤC LỤC TÀI LIỆU KỸ THUẬT VÀ HƯỚNG DẪN TRA CỨU HỆ THỐNG

> **Cổng thông tin tài liệu chính thức của dự án SmartGrow AI**  
> **Cập nhật:** 2026-10-27  
> **Phiên bản hệ thống:** 1.0.0

---

## 1. LỘ TRÌNH ĐỌC TÀI LIỆU THEO VAI TRÒ (RECOMMENDED READING PATH)

### 1.1. Dành cho Nhà phát triển mới gia nhập (New Developer / Onboarding)
1. [README.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/README.md): Tổng quan nhanh, kiến trúc và cách chạy dự án trong 5 phút.
2. [docs/FINAL_SYSTEM_AUDIT_REPORT.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FINAL_SYSTEM_AUDIT_REPORT.md): Báo cáo kiểm toán cuối cùng, khắc phục timezone/logging, sửa export CSV và xác minh persistence qua vòng đời container.
3. [docs/FULL_PROJECT_REAUDIT.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FULL_PROJECT_REAUDIT.md): Báo cáo kiểm toán toàn diện hiện trạng code, test results, và các điểm lưu ý kỹ thuật.
4. [docs/COMPLETE_CODEBASE_FUNCTIONAL_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/COMPLETE_CODEBASE_FUNCTIONAL_GUIDE.md): Giải thích chi tiết từng chức năng, module, API router và hàm nghiệp vụ.
5. [docs/END_TO_END_EXECUTION_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/END_TO_END_EXECUTION_GUIDE.md): Lần theo luồng dữ liệu dọc từ Vue 3 → FastAPI → SQLite.

### 1.2. Dành cho Kỹ sư Cơ sở Dữ liệu & DevOps (DBA / DevOps Engineer)
1. [docs/DATABASE_RUNTIME_AND_DATA_FLOW.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/DATABASE_RUNTIME_AND_DATA_FLOW.md): Phân tích chi tiết vị trí database thật trong Docker, volume mounts, schema 17 bảng và rủi ro persistence.
2. [docs/DATABASE_ACCESS_AND_EDITING_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/DATABASE_ACCESS_AND_EDITING_GUIDE.md): Hướng dẫn mở, xem, truy vấn, sao lưu và sửa database đang chạy bằng DB Browser for SQLite hoặc CLI.
3. [docs/DOCKER_RUNTIME_AND_DEBUG_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/DOCKER_RUNTIME_AND_DEBUG_GUIDE.md): Quản trị mạng container, port forwarding, biến môi trường và lệnh debug.

### 1.3. Dành cho Kỹ sư Nhúng & Phần cứng (Embedded / IoT Engineer)
1. [docs/HARDWARE_WIRING_AND_GPIO.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/HARDWARE_WIRING_AND_GPIO.md): Sơ đồ chân GPIO ESP32-S3, đấu nối cảm biến đất, DHT11, LDR và relay máy bơm.
2. [docs/FIRMWARE_CONFIGURATION_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FIRMWARE_CONFIGURATION_GUIDE.md): Cấu hình FreeRTOS tasks, Wi-Fi, NVS fallback, Safety Gate 3 lớp và lệnh command poller.
3. [docs/ML_EDGE_AI_DEEP_DIVE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/ML_EDGE_AI_DEEP_DIVE.md): Chi tiết mô hình Decision Tree TinyML transpile sang C và kiểm thử parity.

### 1.4. Dành cho Kỹ sư QA & Kiểm thử Phần mềm (QA / Automation Engineer)
1. [docs/FRONTEND_CODE_AND_UI_AUDIT.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FRONTEND_CODE_AND_UI_AUDIT.md): Kiểm toán mã nguồn giao diện Vue 3, TypeScript, routing guards và state management.
2. [docs/FRONTEND_RUN_AND_TEST_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FRONTEND_RUN_AND_TEST_GUIDE.md): Các bước khởi chạy và kịch bản kiểm thử giao diện trực quan.
3. [docs/testing.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/testing.md) & [docs/verification-matrix.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/verification-matrix.md): Ma trận kiểm thử toàn hệ thống (Unit tests, Integration tests, HIL simulation).

---

## 2. BẢNG TRA CỨU TOÀN BỘ DANH MỤC TÀI LIỆU

| Tên tài liệu | Phân loại | Mục đích chính |
|---|---|---|
| [FULL_PROJECT_REAUDIT.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FULL_PROJECT_REAUDIT.md) | Báo cáo kiểm toán | Báo cáo tổng kiểm toán toàn bộ repository với bằng chứng thực tế từ runtime, test suite và mã nguồn. |
| [DATABASE_RUNTIME_AND_DATA_FLOW.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/DATABASE_RUNTIME_AND_DATA_FLOW.md) | Cơ sở dữ liệu | Xác định chính xác vị trí SQLite runtime trong container, bảng biểu, ERD, và phân tích rủi ro persistence. |
| [DATABASE_ACCESS_AND_EDITING_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/DATABASE_ACCESS_AND_EDITING_GUIDE.md) | Hướng dẫn vận hành | Hướng dẫn thao tác mở, xem dữ liệu bằng DB Browser for SQLite, truy vấn CLI và sửa đổi dữ liệu an toàn. |
| [FRONTEND_CODE_AND_UI_AUDIT.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FRONTEND_CODE_AND_UI_AUDIT.md) | Frontend | Kiểm toán 12 views Vue 3, Pinia auth store, Axios interceptors, Vite proxy và khả năng chịu lỗi. |
| [FRONTEND_RUN_AND_TEST_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FRONTEND_RUN_AND_TEST_GUIDE.md) | Frontend | Hướng dẫn khởi chạy frontend trên Windows qua Docker hoặc host, kèm checklist kiểm thử UI. |
| [END_TO_END_EXECUTION_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/END_TO_END_EXECUTION_GUIDE.md) | Tích hợp hệ thống | Truy vết chi tiết 3 luồng tích hợp end-to-end: Auth, Telemetry Ingestion và Điều khiển tưới an toàn. |
| [COMPLETE_CODEBASE_FUNCTIONAL_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/COMPLETE_CODEBASE_FUNCTIONAL_GUIDE.md) | Mã nguồn | Cẩm nang giải thích từng module, API endpoint, hàm nghiệp vụ và chuỗi gọi hàm của 5 phân hệ. |
| [DOCKER_RUNTIME_AND_DEBUG_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/DOCKER_RUNTIME_AND_DEBUG_GUIDE.md) | Triển khai | Hướng dẫn Docker Compose, mạng container, phân giải DNS nội bộ, xem log và cứu hộ container. |
| [HARDWARE_WIRING_AND_GPIO.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/HARDWARE_WIRING_AND_GPIO.md) | Phần cứng | Hướng dẫn đấu nối GPIO, cách ly relay, nguồn cấp cảm biến và bảo vệ quá áp cho ESP32-S3. |
| [FIRMWARE_CONFIGURATION_GUIDE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/FIRMWARE_CONFIGURATION_GUIDE.md) | Firmware | Hướng dẫn cấu hình FreeRTOS, Safety Gate, Command Queue TTL và hiệu chuẩn cảm biến. |
| [ML_EDGE_AI_DEEP_DIVE.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/ML_EDGE_AI_DEEP_DIVE.md) | Machine Learning | Phân tích chuyên sâu kiến trúc Decision Tree C transpiled, ma trận nhầm lẫn và kiểm thử nhiễu. |
| [PROJECT_DOCUMENTATION_INDEX.md](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/PROJECT_DOCUMENTATION_INDEX.md) | Mục lục | Bản đồ tra cứu toàn bộ tài liệu trong dự án. |
