import asyncio
from datetime import datetime, timezone, timedelta
from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from backend.app.core.config import settings
from backend.app.core.database import engine, Base, SessionLocal
from backend.app.core.security import get_password_hash
from backend.app.models.entities import User, AIModel, Device
from backend.app.api.v1 import (
    auth, greenhouses, zones, devices, telemetry, edge_ai,
    automation, irrigation, alerts, analytics, system, ota
)

async def periodic_device_health_checker():
    """Background task to detect offline devices"""
    while True:
        try:
            await asyncio.sleep(15)
            db = SessionLocal()
            now = datetime.now(timezone.utc)
            timeout_threshold = now - timedelta(seconds=settings.DEVICE_HEARTBEAT_TIMEOUT_SECONDS)
            
            # Find online devices that haven't sent telemetry or heartbeat in timeout window
            stale_devices = db.query(Device).filter(
                Device.status == "online",
                Device.last_seen_at < timeout_threshold
            ).all()

            for dev in stale_devices:
                dev.status = "offline"
            
            if stale_devices:
                db.commit()
            db.close()
        except asyncio.CancelledError:
            break
        except Exception:
            pass

from sqlalchemy import text

@asynccontextmanager
async def lifespan(app: FastAPI):
    # 1. Initialize Tables & Migrations
    Base.metadata.create_all(bind=engine)
    try:
        with engine.begin() as conn:
            result = conn.execute(text("PRAGMA table_info(device_commands)")).fetchall()
            cols = [r[1] for r in result]
            if cols and "idempotency_key" not in cols:
                conn.execute(text("ALTER TABLE device_commands ADD COLUMN idempotency_key VARCHAR(128)"))

            # Migrate sensor_readings columns
            result = conn.execute(text("PRAGMA table_info(sensor_readings)")).fetchall()
            sr_cols = {r[1]: r for r in result}
            if sr_cols:
                if "soil_valid" not in sr_cols:
                    conn.execute(text("ALTER TABLE sensor_readings ADD COLUMN soil_valid BOOLEAN DEFAULT 1"))
                if "dht_valid" not in sr_cols:
                    conn.execute(text("ALTER TABLE sensor_readings ADD COLUMN dht_valid BOOLEAN DEFAULT 1"))
                if "light_valid" not in sr_cols:
                    conn.execute(text("ALTER TABLE sensor_readings ADD COLUMN light_valid BOOLEAN DEFAULT 1"))
                if sr_cols.get("soil_moisture_pct", (0, 0, 0, 0))[3] == 1:
                    conn.execute(text("CREATE TABLE sensor_readings_new (id INTEGER PRIMARY KEY, device_id VARCHAR(64) NOT NULL, zone_id INTEGER, sequence INTEGER NOT NULL, boot_id VARCHAR(64), soil_moisture_pct FLOAT, temperature_c FLOAT, humidity_pct FLOAT, light_level_pct FLOAT, soil_valid BOOLEAN NOT NULL DEFAULT 1, dht_valid BOOLEAN NOT NULL DEFAULT 1, light_valid BOOLEAN NOT NULL DEFAULT 1, ai_class INTEGER DEFAULT 0, ai_confidence FLOAT DEFAULT 1.0, model_version VARCHAR(50) DEFAULT 'v1.0', firmware_version VARCHAR(50) DEFAULT '1.0.0', pump_state BOOLEAN DEFAULT 0, operating_mode VARCHAR(20) DEFAULT 'AUTO', health_status VARCHAR(20) DEFAULT 'HEALTHY', recorded_at DATETIME NOT NULL, FOREIGN KEY(device_id) REFERENCES devices (device_id) ON DELETE CASCADE)"))
                    conn.execute(text("INSERT INTO sensor_readings_new SELECT id, device_id, zone_id, sequence, boot_id, soil_moisture_pct, temperature_c, humidity_pct, light_level_pct, soil_valid, dht_valid, light_valid, ai_class, ai_confidence, model_version, firmware_version, pump_state, operating_mode, health_status, recorded_at FROM sensor_readings"))
                    conn.execute(text("DROP TABLE sensor_readings"))
                    conn.execute(text("ALTER TABLE sensor_readings_new RENAME TO sensor_readings"))
                    conn.execute(text("CREATE INDEX idx_sensor_device_time ON sensor_readings (device_id, recorded_at)"))

            # Migrate ai_inference_events columns
            result_ai = conn.execute(text("PRAGMA table_info(ai_inference_events)")).fetchall()
            ai_cols = {r[1]: r for r in result_ai}
            if ai_cols and ai_cols.get("soil_moisture_pct", (0, 0, 0, 0))[3] == 1:
                conn.execute(text("CREATE TABLE ai_inference_events_new (id INTEGER PRIMARY KEY, device_id VARCHAR(64) NOT NULL, zone_id INTEGER, recorded_at DATETIME NOT NULL, soil_moisture_pct FLOAT, temperature_c FLOAT, humidity_pct FLOAT, light_level_pct FLOAT, predicted_class INTEGER NOT NULL, predicted_label VARCHAR(50) NOT NULL, confidence FLOAT DEFAULT 1.0, model_version VARCHAR(50) DEFAULT 'v1.0', safety_override BOOLEAN DEFAULT 0, final_action VARCHAR(50) DEFAULT 'NONE')"))
                conn.execute(text("INSERT INTO ai_inference_events_new SELECT id, device_id, zone_id, recorded_at, soil_moisture_pct, temperature_c, humidity_pct, light_level_pct, predicted_class, predicted_label, confidence, model_version, safety_override, final_action FROM ai_inference_events"))
                conn.execute(text("DROP TABLE ai_inference_events"))
                conn.execute(text("ALTER TABLE ai_inference_events_new RENAME TO ai_inference_events"))
    except Exception as e:
        print(f"Database migration warning: {e}")

    # 2. Seed Initial Admin User if not exists
    db = SessionLocal()
    admin = db.query(User).filter(User.email == settings.INITIAL_ADMIN_EMAIL).first()
    if not admin:
        admin_user = User(
            email=settings.INITIAL_ADMIN_EMAIL,
            hashed_password=get_password_hash(settings.INITIAL_ADMIN_PASSWORD),
            full_name=settings.INITIAL_ADMIN_FULL_NAME,
            role="admin",
            is_active=True,
            is_superuser=True
        )
        db.add(admin_user)
        db.commit()

    # 3. Seed Default AI Model Metadata
    model = db.query(AIModel).filter(AIModel.model_version == "v1.0.0-dt").first()
    if not model:
        ai_model = AIModel(
            model_version="v1.0.0-dt",
            model_type="DecisionTree TinyML C",
            accuracy=0.985,
            f1_score=0.982,
            input_features="soil_moisture,temp,humidity,light",
            is_active=True
        )
        db.add(ai_model)
        db.commit()

    db.close()

    # 4. Launch Background Task
    health_task = asyncio.create_task(periodic_device_health_checker())
    yield
    health_task.cancel()
    try:
        await health_task
    except asyncio.CancelledError:
        pass

app = FastAPI(
    title=settings.APP_NAME,
    description="Intelligent Greenhouse Monitoring, Edge AI and Irrigation Management Platform",
    version="1.0.0",
    docs_url="/api/docs",
    redoc_url="/api/redoc",
    openapi_url="/api/openapi.json",
    lifespan=lifespan
)

# CORS Middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.ALLOWED_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include all API v1 Routers
app.include_router(auth.router, prefix=settings.API_V1_STR)
app.include_router(greenhouses.router, prefix=settings.API_V1_STR)
app.include_router(zones.router, prefix=settings.API_V1_STR)
app.include_router(devices.router, prefix=settings.API_V1_STR)
app.include_router(telemetry.router, prefix=settings.API_V1_STR)
app.include_router(edge_ai.router, prefix=settings.API_V1_STR)
app.include_router(automation.router, prefix=settings.API_V1_STR)
app.include_router(irrigation.router, prefix=settings.API_V1_STR)
app.include_router(alerts.router, prefix=settings.API_V1_STR)
app.include_router(analytics.router, prefix=settings.API_V1_STR)
app.include_router(system.router, prefix=settings.API_V1_STR)
app.include_router(ota.router, prefix=settings.API_V1_STR)

@app.get("/")
def root():
    return {
        "service": settings.APP_NAME,
        "status": "operational",
        "api_docs": "/api/docs",
        "version": "1.0.0"
    }
