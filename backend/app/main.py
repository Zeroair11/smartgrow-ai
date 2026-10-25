import asyncio
from datetime import datetime, timezone, timedelta
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

app = FastAPI(
    title=settings.APP_NAME,
    description="Intelligent Greenhouse Monitoring, Edge AI and Irrigation Management Platform",
    version="1.0.0",
    docs_url="/api/docs",
    redoc_url="/api/redoc",
    openapi_url="/api/openapi.json"
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
        except Exception:
            pass

@app.on_event("startup")
async def on_startup():
    # 1. Initialize Tables
    Base.metadata.create_all(bind=engine)

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
    asyncio.create_task(periodic_device_health_checker())

@app.get("/")
def root():
    return {
        "service": settings.APP_NAME,
        "status": "operational",
        "api_docs": "/api/docs",
        "version": "1.0.0"
    }
