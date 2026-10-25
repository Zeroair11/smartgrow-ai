# SmartGrow AI — Deployment & Production Guide

This guide covers running SmartGrow AI locally, building firmware for the ESP32-S3, and deploying in production using Docker Compose.

---

## 1. Local Development Quickstart

### Prerequisites
- Python 3.11+
- Node.js 20+ / npm 10+
- GCC (MinGW / MSYS2 ucrt64 on Windows, or GCC on Linux)

### Step 1: Backend Setup
```powershell
# Create & activate venv
py -3.11 -m venv backend/.venv
.\backend\.venv\Scripts\Activate.ps1

# Install requirements
pip install -r backend/requirements.txt

# Start FastAPI development server
uvicorn backend.app.main:app --reload --host 127.0.0.1 --port 8000
```
- API Docs: `http://127.0.0.1:8000/api/docs`
- Default Admin Login: `admin@smartgrow.local` / `Admin@SmartGrow2026!`

### Step 2: Frontend Setup
```powershell
cd frontend
npm install
npm run dev
```
- Web Application UI: `http://localhost:5173`

### Step 3: Firmware Build (PlatformIO)
```powershell
cd firmware
pio run -e esp32s3_n16r8
# Flash over USB
pio run -e esp32s3_n16r8 --target upload
```

---

## 2. Docker Compose Deployment

Run the complete platform (Backend + Frontend) with one command:

```powershell
docker compose -f deploy/docker-compose.yml up --build -d
```

- Web Console: `http://localhost:5173`
- Backend API: `http://localhost:8000`

---

## 3. Production Hardening Checklist
1. **Change Default Secrets:** Update `SECRET_KEY` and `DEVICE_REGISTRATION_TOKEN` in `.env`.
2. **Switch to PostgreSQL:** Change `DATABASE_URL` in `.env`:
   ```env
   DATABASE_URL=postgresql://user:password@postgres_host:5432/smartgrow
   ```
3. **HTTPS / Reverse Proxy:** Put Nginx, Caddy, or Traefik in front of the services to terminate TLS/HTTPS.
4. **CORS Restrictions:** Restrict `ALLOWED_ORIGINS` to production domains only.
