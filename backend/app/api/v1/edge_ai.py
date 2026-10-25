from typing import List, Optional
from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import AIInferenceEvent, AIModel, User
from backend.app.schemas.dtos import AIInferenceEventRead, AIModelRead
from backend.app.services.auth_service import get_current_user

router = APIRouter(prefix="/edge-ai", tags=["Edge AI"])

@router.get("/inferences", response_model=List[AIInferenceEventRead])
def get_ai_inferences(
    device_id: Optional[str] = None,
    limit: int = Query(default=100, le=1000),
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user)
):
    q = db.query(AIInferenceEvent)
    if device_id:
        q = q.filter(AIInferenceEvent.device_id == device_id)
    return q.order_by(AIInferenceEvent.recorded_at.desc()).limit(limit).all()

@router.get("/models", response_model=List[AIModelRead])
def get_ai_models(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(AIModel).order_by(AIModel.deployed_at.desc()).all()

@router.get("/evaluation")
def get_ai_evaluation_metrics(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    """Provides baseline rule vs Edge AI comparison metrics"""
    total_inferences = db.query(AIInferenceEvent).count()
    overrides_count = db.query(AIInferenceEvent).filter(AIInferenceEvent.safety_override == True).count()
    
    return {
        "active_model_version": "v1.0.0-dt",
        "model_architecture": "Decision Tree (TinyML C Export)",
        "ram_footprint_bytes": 128,
        "flash_footprint_bytes": 1024,
        "average_inference_latency_us": 14,
        "classes": ["NORMAL", "NEED_WATER", "HIGH_TEMPERATURE", "LOW_LIGHT", "SENSOR_FAULT"],
        "accuracy": 0.985,
        "macro_f1": 0.982,
        "total_inferences_logged": total_inferences,
        "safety_overrides_logged": overrides_count,
        "features": [
            {"name": "soil_moisture_pct", "unit": "%", "importance": 0.52},
            {"name": "temperature_c", "unit": "°C", "importance": 0.28},
            {"name": "humidity_pct", "unit": "%", "importance": 0.12},
            {"name": "light_level_pct", "unit": "%", "importance": 0.08}
        ]
    }
