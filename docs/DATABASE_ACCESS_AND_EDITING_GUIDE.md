# HƯỚNG DẪN TRUY CẬP, XEM VÀ CHỈNH SỬA ĐÚNG DATABASE DOCKER RUNTIME

> **Mục tiêu:** Hướng dẫn mở, xem, truy vấn, sao lưu và chỉnh sửa **chính xác database bền vững mà container Docker backend đang sử dụng trên máy tính**, tránh nhầm lẫn với database ở máy host.

---

## 1. PHÂN BIỆT RÕ RÀNG HAI DATABASE TRÊN MÁY TÍNH

Trước khi thao tác, bạn cần nắm rõ:
- **Database thật của ứng dụng đang chạy (Đã lưu bền vững):** Nằm bên trong Docker container `smartgrow-backend` tại đường dẫn `/app/data/smartgrow.db` (gắn kết trực tiếp với Docker Named Volume `deploy_smartgrow_data`). Đây là nơi chứa dữ liệu của thiết bị ESP32-S3 đang online, các tài khoản và telemetry mới nhất.
- **Database trên máy host (`smartgrow-ai\smartgrow.db`):** Là bản sao cũ từ các lần chạy test trước đây trên Windows host. **Nếu bạn mở file này bằng DB Browser for SQLite, các thay đổi sẽ KHÔNG tác động tới backend Docker.**

---

## 2. CÔNG CỤ ĐƯỢC KHUYẾN NGHỊ

1. **DB Browser for SQLite (GUI miễn phí):** Giao diện trực quan xem bảng, duyệt bản ghi, lọc dữ liệu và chạy câu lệnh SQL trên Windows.
2. **VS Code Extension "SQLite Viewer" hoặc "Database Client":** Xem nhanh bảng trực tiếp trong trình biên soạn mã.
3. **Lệnh dòng lệnh Docker (CLI):** Dùng lệnh `docker exec` để truy vấn tức thì mà không cần cài thêm phần mềm ngoài.

---

## 3. CÁCH 1: XUẤT DATABASE TỪ DOCKER RA MÁY HOST ĐỂ XEM BẰNG GUI

Đây là cách an toàn và dễ dàng nhất để duyệt toàn bộ dữ liệu bằng DB Browser for SQLite trên máy Windows.

### Bước 1: Sao chép database từ container ra thư mục làm việc
Mở PowerShell tại thư mục dự án và chạy:
```powershell
# Tạo bản snapshot từ Docker Volume ra file host
docker cp smartgrow-backend:/app/data/smartgrow.db .\runtime_inspect.db
```

### Bước 2: Mở file bằng DB Browser for SQLite
1. Mở phần mềm **DB Browser for SQLite** trên Windows.
2. Bấm **Open Database** (Mở cơ sở dữ liệu).
3. Chọn file `C:\Users\ADMIN\Downloads\smartgrow-ai\runtime_inspect.db`.
4. Chuyển sang tab **Browse Data** (Duyệt dữ liệu) để xem từng bảng:
   - Bảng `devices`: Xem thiết bị `esp32s3-bbd0c4` đang online.
   - Bảng `sensor_readings`: Xem các chỉ số nhiệt độ, độ ẩm đất thời gian thực.
   - Bảng `users`: Xem tài khoản quản trị viên.

### Bước 3: Đồng bộ ngược lại container sau khi chỉnh sửa (nếu có)
Nếu bạn đã chỉnh sửa một bản ghi trong `runtime_inspect.db` (ví dụ: đổi tên zone, đổi ngưỡng độ ẩm) và đã bấm **Write Changes** trong DB Browser:
```powershell
# 1. Sao lưu dự phòng database trong volume trước khi ghi đè
docker exec smartgrow-backend cp /app/data/smartgrow.db /app/data/smartgrow_pre_edit.db

# 2. Copy file đã chỉnh sửa ngược trở lại volume của container
docker cp .\runtime_inspect.db smartgrow-backend:/app/data/smartgrow.db

# 3. Kiểm tra tính toàn vẹn của database
docker exec smartgrow-backend python -c "import sqlite3; conn=sqlite3.connect('/app/data/smartgrow.db'); print('Integrity:', conn.execute('PRAGMA integrity_check;').fetchone()[0])"
```

---

## 4. CÁCH 2: TRUY VẤN VÀ CHỈNH SỬA TRỰC TIẾP QUA DÒNG LỆNH DOCKER

Bạn có thể chạy các câu truy vấn chỉ đọc hoặc cập nhật trực tiếp mà không cần copy file ra ngoài.

### 4.1. Xem danh sách tất cả các bảng và số dòng
```powershell
docker exec smartgrow-backend python -c "
import sqlite3
conn = sqlite3.connect('/app/data/smartgrow.db')
c = conn.cursor()
c.execute(\"SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;\")
for t in c.fetchall():
    cnt = conn.execute(f'SELECT count(*) FROM {t[0]}').fetchone()[0]
    print(f'{t[0]:<25}: {cnt} rows')
"
```

### 4.2. Xem thông tin thiết bị đang kết nối
```powershell
docker exec smartgrow-backend python -c "
import sqlite3
conn = sqlite3.connect('/app/data/smartgrow.db')
c = conn.cursor()
c.execute(\"SELECT device_id, name, status, firmware_version, last_seen_at FROM devices\")
for row in c.fetchall():
    print(row)
"
```

### 4.3. Xem 5 bản ghi telemetry mới nhất
```powershell
docker exec smartgrow-backend python -c "
import sqlite3
conn = sqlite3.connect('/app/data/smartgrow.db')
c = conn.cursor()
c.execute(\"SELECT recorded_at, soil_moisture_pct, temperature_c, humidity_pct, operating_mode FROM sensor_readings ORDER BY id DESC LIMIT 5\")
for row in c.fetchall():
    print(row)
"
```

### 4.4. Ví dụ chỉnh sửa an toàn một bản ghi (Ví dụ: Đổi tên Zone)
Giả sử bạn muốn đổi tên zone có `id = 1`:
```powershell
docker exec smartgrow-backend python -c "
import sqlite3
conn = sqlite3.connect('/app/data/smartgrow.db')
c = conn.cursor()
# Bật kiểm tra khóa ngoại
c.execute('PRAGMA foreign_keys = ON;')
c.execute(\"UPDATE zones SET name = 'Tomato High-Yield Zone A' WHERE id = 1;\")
conn.commit()
print('Rows affected:', c.rowcount)
conn.close()
"
```

Sau đó kiểm tra ngay qua API backend:
```powershell
# Đăng nhập lấy token
$token = (curl.exe -s -X POST "http://localhost:8000/api/v1/auth/login" -H "Content-Type: application/x-www-form-urlencoded" -d "username=admin@smartgrow.local&password=Admin@SmartGrow2026!" | ConvertFrom-Json).access_token

# Gọi API zones để xác nhận backend đã phản ánh dữ liệu mới từ database
curl.exe -s -H "Authorization: Bearer $token" "http://localhost:8000/api/v1/zones"
```

---

## 5. CÁC NGUYÊN TẮC AN TOÀN BẮT BUỘC KHI CHỈNH SỬA DATABASE

1. **Luôn bật PRAGMA foreign_keys = ON:** SQLite mặc định có thể không thực thi ràng buộc khóa ngoại trừ khi được bật rõ ràng. Khi sửa hoặc xóa thủ công, nếu không bật khóa ngoại, bạn có thể tạo ra các bản ghi mồ côi (orphan records).
2. **Không xóa các bảng hệ thống:** Tuyệt đối không xóa bảng `ai_models`, `users` admin, hoặc `devices` đang hoạt động.
3. **Tránh khóa Database (Database Locked):** Khi dùng DB Browser for SQLite hoặc Python script, luôn bấm **Write Changes** hoặc gọi `conn.close()` ngay sau khi hoàn tất. Giữ connection mở có thể khiến FastAPI backend gặp lỗi `sqlite3.OperationalError: database is locked`.
4. **Không chỉnh sửa trường mật khẩu thủ công bằng plain-text:** Trường `hashed_password` trong bảng `users` sử dụng thuật toán băm Bcrypt. Nhập mật khẩu dạng văn bản thô sẽ khiến tài khoản không thể đăng nhập được.
5. **Không chỉnh sửa trực tiếp `device_commands` sang trạng thái `completed`:** Trạng thái lệnh phải do ESP32-S3 gửi gói tin ACK xác nhận sau khi cơ cấu chấp hành vật lý thực sự hoàn thành chu kỳ làm việc.
