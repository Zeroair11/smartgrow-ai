from typing import List, Union
from pydantic import AnyHttpUrl, validator
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    APP_NAME: str = "SmartGrow AI"
    APP_ENV: str = "development"
    DEBUG: bool = True
    API_V1_STR: str = "/api/v1"
    SECRET_KEY: str = "smartgrow-ai-super-secret-production-grade-key-2026"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 60 * 24  # 24 hours
    ALLOWED_ORIGINS: List[str] = [
        "http://localhost:5173",
        "http://localhost:3000",
        "http://127.0.0.1:5173",
        "http://127.0.0.1:3000"
    ]

    DATABASE_URL: str = "sqlite:///./smartgrow.db"

    DEVICE_REGISTRATION_TOKEN: str = "smartgrow-device-enrollment-secret-2026"
    DEVICE_HEARTBEAT_TIMEOUT_SECONDS: int = 60
    COMMAND_DEFAULT_TTL_SECONDS: int = 300

    FIRMWARE_STORAGE_PATH: str = "./firmware_storage"
    MAX_FIRMWARE_SIZE_BYTES: int = 16 * 1024 * 1024

    INITIAL_ADMIN_EMAIL: str = "admin@smartgrow.local"
    INITIAL_ADMIN_PASSWORD: str = "Admin@SmartGrow2026!"
    INITIAL_ADMIN_FULL_NAME: str = "Greenhouse System Administrator"

    class Config:
        env_file = ".env"
        case_sensitive = True

settings = Settings()
