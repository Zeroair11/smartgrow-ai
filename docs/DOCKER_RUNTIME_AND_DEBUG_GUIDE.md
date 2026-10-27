# SMARTGROW AI — HƯỚNG DẪN DOCKER RUNTIME, DEBUG VÀ MẠNG CONTAINER

> **Tài liệu vận hành hệ thống container hóa**  
> **Áp dụng cho:** Docker Engine, Docker Desktop for Windows, Docker Compose v2+

---

## 1. TỔNG QUAN CÁC CONTAINER ĐANG CHẠY

Hệ thống SmartGrow AI được điều phối qua file [deploy/docker-compose.yml](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/deploy/docker-compose.yml):

| Service Name | Container Name | Image Nền tảng | Cổng nội bộ | Cổng Host mapped | Vai trò |
|---|---|---|---|---|---|
| `backend` | `smartgrow-backend` | `deploy-backend` (Python 3.11-slim) | `8000/tcp` | `0.0.0.0:8000` | Cung cấp RESTful API, ORM, background tasks |
| `frontend` | `smartgrow-frontend` | `node:22-alpine` | `5173/tcp` | `0.0.0.0:5173` | Chạy Vite dev server, reverse proxy `/api` |

---

## 2. CẤU HÌNH MẠNG VÀ PHÂN GIẢI DNS NỘI BỘ (NETWORKING)

- **Network Name:** `deploy_default` (Docker Bridge Network)
- **Subnet:** `172.19.0.0/16`
- **Gateway (Host IP nhìn từ container):** `172.19.0.1`
- **IP Containers:**
  - `smartgrow-frontend`: `172.19.0.2`
  - `smartgrow-backend`: `172.19.0.3`
- **DNS nội bộ giữa các container:** Container `smartgrow-frontend` có thể kết nối trực tiếp tới `http://backend:8000` thông qua tên service `backend`.

---

## 3. CÁC LỆNH VẬN HÀNH DOCKER CƠ BẢN DÀNH CHO WINDOWS (POWERSHELL)

### 3.1. Khởi động và kiểm tra trạng thái
```powershell
# Chuyển vào thư mục dự án
cd C:\Users\ADMIN\Downloads\smartgrow-ai

# Khởi chạy toàn bộ dịch vụ ngầm
docker compose -f deploy/docker-compose.yml up -d

# Xem danh sách trạng thái và cổng mở
docker compose -f deploy/docker-compose.yml ps
```

### 3.2. Xem nhật ký (Logs) thời gian thực
```powershell
# Xem log backend (theo dõi requests API và telemetry gửi về)
docker logs --tail 50 -f smartgrow-backend

# Xem log frontend (theo dõi Vite build và proxy events)
docker logs --tail 50 -f smartgrow-frontend
```

### 3.3. Dừng hệ thống an toàn
```powershell
# Tạm dừng các container (không xóa container)
docker compose -f deploy/docker-compose.yml stop

# Dừng và gỡ bỏ container nhưng GIỮ NGUYÊN volume dữ liệu
docker compose -f deploy/docker-compose.yml down
```

> ⚠️ **CẢNH BÁO QUAN TRỌNG:** Tuyệt đối **KHÔNG** sử dụng `docker compose down -v` hoặc `docker volume prune` vì sẽ xóa sạch volume lưu trữ dữ liệu.

---

## 4. HƯỚNG DẪN DEBUG VÀ KHẮC PHỤC SỰ CỐ CONTAINER

### 4.1. Kiểm tra kết nối từ Frontend sang Backend
Mở shell bên trong container frontend và thử gọi API backend:
```powershell
docker exec -it smartgrow-frontend wget -qO- http://backend:8000/
```
Nếu kết quả trả về `{"service":"SmartGrow AI","status":"operational"...}`, đường truyền mạng nội bộ giữa 2 container hoàn toàn bình thường.

### 4.2. Kiểm tra truy cập file hệ thống bên trong Backend
Truy cập vào shell của container backend để kiểm tra file database:
```powershell
docker exec -it smartgrow-backend ls -lh /app/data/smartgrow.db
```

### 4.3. Kiểm tra biến môi trường thực tế đang áp dụng
```powershell
docker exec smartgrow-backend env | Select-String "DATABASE_URL|ALLOWED_ORIGINS|DEBUG"
```

### 4.4. Khắc phục lỗi khi Container không khởi động được
Nếu một container bị crash hoặc báo lỗi `Exited (1)`:
```powershell
# Kiểm tra log lỗi chi tiết lúc khởi động
docker logs --tail 100 smartgrow-backend

# Kiểm tra Docker Desktop for Windows có bị quá tải bộ nhớ (RAM/WSL2) không
wsl --status
```
