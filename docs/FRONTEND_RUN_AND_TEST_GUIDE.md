# HƯỚNG DẪN KHỞI CHẠY VÀ KIỂM THỬ GIAO DIỆN FRONTEND

> **Tài liệu hướng dẫn thao tác dành cho nhà phát triển và kiểm thử viên trên hệ điều hành Windows**  
> **Cổng truy cập đã xác minh:** Frontend `http://localhost:5173` | Backend `http://localhost:8000`

---

## 1. ĐIỀU KIỆN TIÊN QUYẾT

Trước khi chạy, máy tính của bạn cần có:
- **Node.js:** Phiên bản `>= 18.0.0` (Hiện tại container dùng Node 22.23.4 Alpine).
- **Docker Desktop for Windows:** (Nếu chạy qua Docker) Đảm bảo Docker daemon đang ở trạng thái `running`.
- **Trình duyệt web:** Google Chrome, Microsoft Edge, Firefox hoặc Brave.
- **PowerShell:** Mở bằng Windows Terminal hoặc PowerShell thông thường.

---

## 2. PHƯƠNG ÁN 1: CHẠY BẰNG DOCKER COMPOSE (ĐƯỢC KHUYẾN NGHỊ)

Đây là cách dự án đang được triển khai thực tế trên máy tính của bạn. Cả frontend và backend chạy đồng thời trong cùng một mạng ảo Docker.

### Bước 1: Mở PowerShell và di chuyển đến thư mục dự án
```powershell
cd C:\Users\ADMIN\Downloads\smartgrow-ai
```

### Bước 2: Kiểm tra trạng thái các container hiện có
```powershell
docker compose -f deploy/docker-compose.yml ps
```
Nếu trạng thái hiển thị `Up` cho cả `smartgrow-backend` và `smartgrow-frontend`, hệ thống đã sẵn sàng.

### Bước 3: Khởi động dịch vụ (nếu chưa chạy)
```powershell
docker compose -f deploy/docker-compose.yml up -d
```

### Bước 4: Kiểm tra log của frontend
```powershell
docker logs --tail 20 -f smartgrow-frontend
```
Khi thấy dòng thông báo sau, frontend đã sẵn sàng nhận kết nối:
```
  VITE v5.4.21  ready in ... ms

  ➜  Local:   http://localhost:5173/
  ➜  Network: http://172.19.0.2:5173/
```

---

## 3. PHƯƠNG ÁN 2: CHẠY ĐỘC LẬP FRONTEND TRÊN MÁY HOST

Nếu bạn muốn chỉnh sửa giao diện với tốc độ phản hồi HMR cao nhất mà không phụ thuộc vào Docker container của frontend:

### Bước 1: Di chuyển vào thư mục `frontend`
```powershell
cd C:\Users\ADMIN\Downloads\smartgrow-ai\frontend
```

### Bước 2: Cài đặt dependencies (chỉ cần chạy lần đầu)
```powershell
npm install
```

### Bước 3: Khởi chạy Vite dev server
```powershell
npm run dev
```
Trình duyệt sẽ mở tại `http://localhost:5173`. Vite proxy sẽ tự động chuyển tiếp các cuộc gọi `/api` về backend đang chạy ở `http://127.0.0.1:8000`.

---

## 4. BẢNG KIỂM TRA SỨC KHỎE HỆ THỐNG TRƯỚC KHI MỞ TRÌNH DUYỆT

Chạy các lệnh PowerShell sau để đảm bảo backend và frontend phản hồi tốt:

### 4.1. Kiểm tra Backend API
```powershell
curl.exe -s http://localhost:8000/
```
**Kết quả kỳ vọng:**
```json
{"service":"SmartGrow AI","status":"operational","api_docs":"/api/docs","version":"1.0.0"}
```

### 4.2. Kiểm tra Swagger Documentation
```powershell
curl.exe -s -I http://localhost:8000/api/docs
```
**Kết quả kỳ vọng:** `HTTP/1.1 200 OK`

### 4.3. Kiểm tra Frontend HTTP Response
```powershell
curl.exe -s -I http://localhost:5173/
```
**Kết quả kỳ vọng:** `HTTP/1.1 200 OK`

### 4.4. Kiểm tra Vite Proxy Đăng nhập
```powershell
curl.exe -s -X POST "http://localhost:5173/api/v1/auth/login" `
  -H "Content-Type: application/x-www-form-urlencoded" `
  -d "username=admin@smartgrow.local&password=Admin@SmartGrow2026!"
```
**Kết quả kỳ vọng:** Trả về JSON chứa `access_token` hợp lệ.

---

## 5. HƯỚNG DẪN KIỂM THỬ TRỰC QUAN TRÊN TRÌNH DUYỆT (MANUAL TESTING)

Mở trình duyệt Google Chrome hoặc Microsoft Edge và thực hiện tuần tự các bước sau:

1. **Mở URL:** Nhập `http://localhost:5173` vào thanh địa chỉ.
2. **Xác nhận chuyển hướng đăng nhập:** Bạn sẽ tự động được điều hướng tới `http://localhost:5173/login`.
3. **Nhập tài khoản quản trị:**
   - **Email:** `admin@smartgrow.local`
   - **Mật khẩu:** `Admin@SmartGrow2026!`
   - Bấm nút **"Sign in to Dashboard"**.
4. **Kiểm tra Dashboard (`/`):**
   - 4 thẻ thống kê trên cùng (Active Greenhouses, Online Devices, Active Alerts, System Status) hiển thị đầy đủ số liệu.
   - Bảng **Recent Devices** hiển thị thiết bị `esp32s3-bbd0c4` kèm chấm tròn màu xanh lá (Online) và MAC address `b8:1f:3f:bb:d0:c4`.
   - Biểu đồ cảm biến vẽ đường dữ liệu độ ẩm đất và nhiệt độ.
5. **Duyệt qua các phân hệ (Sidebar bên trái):**
   - **Greenhouses & Zones (`/greenhouses`):** Xem danh sách nhà kính và khu vực.
   - **ESP32-S3 Devices (`/devices`):** Xem danh mục phần cứng. *(Lưu ý an toàn: Không bấm nút Reboot hoặc Pump kích hoạt thiết bị thật)*.
   - **Live Monitoring (`/monitoring`):** Xem đồ thị cảm biến cập nhật theo chu kỳ.
   - **Edge AI Center (`/edge-ai`):** Xem mô hình `v1.0.0-dt` và nhật ký suy luận.
   - **Alerts & Incidents (`/alerts`):** Xem các cảnh báo hệ thống.
   - **Users & Audit (`/users-audit`):** Xem danh sách người dùng và log kiểm toán.
6. **Mở F12 (Developer Tools):**
   - Vào tab **Console:** Xác nhận không có lỗi JavaScript đỏ (Uncaught Exceptions).
   - Vào tab **Network:** Lọc `Fetch/XHR`, xác nhận các yêu cầu `/api/v1/...` đều trả về `200 OK`.

---

## 6. HƯỚNG DẪN DỪNG VÀ DỌN DẸP DỊCH VỤ AN TOÀN

Khi hoàn tất phiên làm việc, dừng dịch vụ đúng cách để bảo toàn dữ liệu:

```powershell
# Dừng container an toàn (KHÔNG dùng cờ -v để tránh mất database)
docker compose -f deploy/docker-compose.yml stop

# Hoặc tắt container hoàn toàn (bảo tồn volume):
docker compose -f deploy/docker-compose.yml down
```

> ⚠️ **CẢNH BÁO QUAN TRỌNG:** Tuyệt đối **KHÔNG** chạy `docker compose down -v` vì lệnh này sẽ xóa toàn bộ volume và làm mất database của dự án.
