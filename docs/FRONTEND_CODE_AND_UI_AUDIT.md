# SMARTGROW AI — KIỂM TOÁN CHI TIẾT MÃ NGUỒN VÀ GIAO DIỆN FRONTEND

> **Phân hệ:** Frontend Single Page Application (SPA)  
> **Công nghệ:** Vue 3, TypeScript, Vite 5.4, Pinia, Tailwind CSS  
> **Trạng thái:** `PASS` (Build production 100% không lỗi, Type-check sạch, Routing & Auth Guards hoạt động đúng)

---

## 1. CẤU HÌNH, PACKAGE MANAGER VÀ KHỞI TẠO DỰ ÁN

### 1.1. Package Manager và Scripts
- **Package Manager:** `npm` (được xác thực qua [package-lock.json](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/package-lock.json)).
- **Scripts trong `package.json`:**
  - `npm run dev`: Chạy Vite development server với HMR (Hot Module Replacement) tại port 5173.
  - `npm run build`: Chạy `vue-tsc --noEmit && vite build`. Đã được kiểm thử thực tế và hoàn thành sau 47.20s với **115 modules transformed**, 0 lỗi TypeScript.
  - `npm run preview`: Chạy local web server thử nghiệm thư mục build `dist/`.

### 1.2. Entry Point và Bootstrap
- **File:** [frontend/src/main.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/main.ts)
- **Logic:**
  1. Tạo ứng dụng Vue thông qua `createApp(App)`.
  2. Đăng ký Pinia store: `app.use(createPinia())`.
  3. Đăng ký Vue Router: `app.use(router)`.
  4. Nạp CSS toàn cục: `import './assets/main.css'`.
  5. Mount vào thẻ DOM gốc: `app.mount('#app')`.

### 1.3. Cấu hình Vite và Cơ chế Proxy
- **File:** [frontend/vite.config.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/vite.config.ts)
- **Cơ chế Reverse Proxy:**
  ```typescript
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: process.env.BACKEND_URL || 'http://127.0.0.1:8000',
        changeOrigin: true
      }
    }
  }
  ```
- **Ý nghĩa vận hành:** Mọi yêu cầu HTTP bắt đầu bằng `/api` gửi từ trình duyệt sẽ được máy chủ Vite proxy ngầm sang backend FastAPI. Trong Docker container, biến môi trường `BACKEND_URL=http://backend:8000` đảm bảo định tuyến trực tiếp trong mạng Docker bridge `deploy_default`, giải quyết triệt để lỗi CORS và tránh để lộ port backend ra mạng ngoài.

---

## 2. KIẾN TRÚC ĐIỀU HƯỚNG VÀ BẢO MẬT (ROUTING & GUARDS)

### 2.1. Danh mục Routes và Navigation
Được định nghĩa tập trung tại [frontend/src/router/index.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/router/index.ts):

| Đường dẫn (URL) | Tên Route | Component View | Quyền truy cập | Tiêu đề màn hình |
|---|---|---|---|---|
| `/login` | `login` | `LoginView.vue` | Công khai (`public: true`) | Đăng nhập hệ thống |
| `/` | `dashboard` | `DashboardView.vue` | Đăng nhập (`auth`) | Dashboard & Quick Telemetry |
| `/greenhouses` | `greenhouses` | `GreenhousesView.vue` | Đăng nhập (`auth`) | Greenhouses & Zones |
| `/devices` | `devices` | `DevicesView.vue` | Đăng nhập (`auth`) | ESP32-S3 Device Management |
| `/monitoring` | `monitoring` | `MonitoringView.vue` | Đăng nhập (`auth`) | Live Sensor Telemetry |
| `/edge-ai` | `edge-ai` | `EdgeAiView.vue` | Đăng nhập (`auth`) | Edge AI Center & TinyML |
| `/automation` | `automation` | `AutomationView.vue` | Đăng nhập (`auth`) | Automation Rules & Engine |
| `/irrigation` | `irrigation` | `IrrigationView.vue` | Đăng nhập (`auth`) | Irrigation & Pump Safety Control |
| `/alerts` | `alerts` | `AlertsView.vue` | Đăng nhập (`auth`) | Alerts & Incidents |
| `/schedules` | `schedules` | `ScheduleView.vue` | Đăng nhập (`auth`) | Schedules & Maintenance |
| `/analytics` | `analytics` | `AnalyticsView.vue` | Đăng nhập (`auth`) | Analytics & Reports |
| `/settings` | `settings` | `SettingsView.vue` | Đăng nhập (`auth`) | System Settings & Calibration |
| `/users-audit` | `users-audit` | `UsersAuditView.vue` | Đăng nhập (`auth`) | Users & Audit Logs |

### 2.2. Navigation Guard (`router.beforeEach`)
- Kiểm tra trạng thái `authStore.isAuthenticated`:
  - Nếu người dùng chưa có JWT token và cố tình truy cập bất kỳ route nào không có `meta.public: true` -> Điều hướng ngay lập tức về `/login`.
  - Nếu người dùng đã có token hợp lệ mà truy cập `/login` -> Tự động chuyển hướng về `/` (Dashboard).

---

## 3. STATE MANAGEMENT VÀ API CLIENT

### 3.1. Pinia Store: `authStore.ts`
- **File:** [frontend/src/stores/authStore.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/stores/authStore.ts)
- **State:**
  - `token`: Chuỗi JWT Bearer token lưu trong `localStorage('smartgrow_token')`.
  - `user`: Thông tin profile (`email`, `role`, `is_superuser`, `full_name`) lưu trong `localStorage('smartgrow_user')`.
- **Getters tính toán:**
  - `isAuthenticated`: `true` khi token tồn tại.
  - `isAdmin`: `true` khi `role === 'admin'` hoặc `is_superuser === true`.
  - `isOperator`: `true` khi `role` thuộc `['admin', 'operator']`.
- **Hành động (Actions):**
  - `login(username, password)`: Gửi form urlencoded tới `/auth/login`, lưu token, tự động fetch profile từ `/auth/me`.
  - `logout()`: Xóa token và user khỏi state và localStorage.
  - `fetchCurrentUser()`: Xác thực lại token khi reload trình duyệt.

### 3.2. Axios API Client: `client.ts`
- **File:** [frontend/src/api/client.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/api/client.ts)
- **Base URL:** `/api/v1` (tự động đi qua Vite proxy tới backend).
- **Request Interceptor:** Tự động đính kèm header `Authorization: Bearer <token>` vào mọi yêu cầu nếu có token.
- **Response Interceptor:** Bắt mã lỗi `401 Unauthorized`. Khi token hết hạn hoặc không hợp lệ, client tự động dọn dẹp localStorage và điều hướng về trang `/login`.

---

## 4. PHÂN TÍCH CHI TIẾT TỪNG MÀN HÌNH CHỨC NĂNG (VIEWS)

### 4.1. `DashboardView.vue` (Tổng quan & Giám sát nhanh)
- **Trách nhiệm:** Hiển thị 4 thẻ thống kê chính (KPIs), biểu đồ xu hướng cảm biến 24h, danh sách thiết bị gần đây và trạng thái cảnh báo nóng.
- **APIs được gọi:**
  - `GET /api/v1/analytics/dashboard-summary`
  - `GET /api/v1/telemetry/history?limit=30`
  - `GET /api/v1/devices`
- **Dữ liệu hiển thị:** Số nhà kính hoạt động, số thiết bị online (`esp32s3-bbd0c4`), cảnh báo chưa xử lý, biểu đồ độ ẩm đất/nhiệt độ.
- **Trạng thái:** Có hiệu ứng loading skeleton, thông báo rỗng khi chưa có dữ liệu.

### 4.2. `DevicesView.vue` (Quản lý Thiết bị ESP32-S3)
- **Trách nhiệm:** Liệt kê bảng thiết bị, trạng thái online/offline, phiên bản firmware, địa chỉ MAC, gán zone và thao tác gửi lệnh quản trị.
- **APIs được gọi:**
  - `GET /api/v1/devices`
  - `POST /api/v1/devices/register`
  - `PUT /api/v1/devices/{id}/zone`
  - `POST /api/v1/devices/{id}/commands` (Chỉ hiển thị cho Admin/Operator)
- **Bảo vệ an toàn:** Nút thao tác kích hoạt bơm hoặc reboot được bọc trong modal xác nhận 2 bước, ngăn chặn bấm nhầm.

### 4.3. `MonitoringView.vue` (Giám sát Telemetry Thời gian thực)
- **Trách nhiệm:** Trực quan hóa dữ liệu đo từ cảm biến theo thời gian thực với biểu đồ đa trục (Chart.js / SVG).
- **APIs được gọi:**
  - `GET /api/v1/telemetry/history?limit=100`
  - `GET /api/v1/telemetry/export` (Tải file CSV lịch sử đo)
- **Cơ chế cập nhật:** Polling định kỳ mỗi 5 giây hoặc cập nhật khi có bản ghi mới.

### 4.4. `EdgeAiView.vue` (Trung tâm Edge AI & TinyML)
- **Trách nhiệm:** Giám sát mô hình Decision Tree đang chạy trên firmware ESP32, hiển thị độ chính xác (98.5%), nhãn phân loại gần nhất (`NEED_WATER`, `NORMAL`, `HIGH_TEMPERATURE`, `LOW_LIGHT`, `SENSOR_FAULT`), và các sự kiện can thiệp an toàn (Safety Overrides).
- **APIs được gọi:**
  - `GET /api/v1/edge-ai/models/active`
  - `GET /api/v1/edge-ai/events?limit=50`

### 4.5. `IrrigationView.vue` (Điều khiển Tưới & An toàn Bơm)
- **Trách nhiệm:** Cho phép kích hoạt chu kỳ tưới thủ công có giới hạn thời gian (tối đa 120s) hoặc đặt lịch tưới.
- **APIs được gọi:**
  - `POST /api/v1/irrigation/start`
  - `POST /api/v1/irrigation/stop`
  - `GET /api/v1/irrigation/history`
- **Ràng buộc an toàn:** Nút "Bắt đầu tưới" bắt buộc nhập thời lượng (10s - 120s). Nếu nhập > 120s, client hiển thị cảnh báo vi phạm Safety Limit trước khi gửi request.

### 4.6. `AlertsView.vue` (Cảnh báo & Sự cố)
- **Trách nhiệm:** Danh sách cảnh báo phân cấp theo mức độ nghiêm trọng: `critical`, `warning`, `info`. Cung cấp nút xác nhận ("Acknowledge") và giải quyết ("Resolve").
- **APIs được gọi:**
  - `GET /api/v1/alerts`
  - `PUT /api/v1/alerts/{id}/resolve`

### 4.7. `UsersAuditView.vue` (Người dùng & Nhật ký Kiểm toán)
- **Trách nhiệm:** Quản lý danh sách tài khoản người dùng và xem nhật ký bảo mật chi tiết (Audit Trail) ghi lại mọi thao tác quan trọng kèm địa chỉ IP và timestamp.
- **APIs được gọi:**
  - `GET /api/v1/auth/users`
  - `POST /api/v1/auth/register`
  - `GET /api/v1/system/audit-logs`
- **Phân quyền:** Chỉ tài khoản có `role === 'admin'` mới có thể xem và thực thi.

---

## 5. XỬ LÝ LỖI VÀ HÀNH VI KHI BACKEND / DATABASE KHÔNG KHẢ DỤNG

1. **Khi Backend tắt hoặc mất kết nối mạng:**
   - Axios client kích hoạt timeout (10 giây).
   - Các view bắt lỗi trong khối `try...catch` và hiển thị Banner thông báo màu đỏ: `"Không thể kết nối tới máy chủ backend. Vui lòng kiểm tra lại dịch vụ."`
   - Giao diện không bị crash (trắng trang); toàn bộ layout, menu và trạng thái cached vẫn hiển thị ổn định.
2. **Khi Token hết hạn hoặc Database bị khóa:**
   - Interceptor phát hiện mã lỗi HTTP 401.
   - Xóa bỏ token rác trong localStorage.
   - Hiển thị Toast thông báo: `"Phiên đăng nhập đã hết hạn. Đang chuyển hướng về trang đăng nhập..."` và điều hướng về `/login`.
