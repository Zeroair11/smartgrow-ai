from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, status, Query
from fastapi.security import OAuth2PasswordRequestForm
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.core.security import create_access_token, get_password_hash
from backend.app.models.entities import User
from backend.app.schemas.dtos import UserLogin, UserRegister, UserUpdate, UserRead, TokenResponse
from backend.app.services.auth_service import authenticate_user, create_user, get_current_user, require_admin
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/auth", tags=["Authentication"])

@router.post("/login", response_model=TokenResponse)
def login(form_data: OAuth2PasswordRequestForm = Depends(), db: Session = Depends(get_db)):
    user = authenticate_user(db, form_data.username, form_data.password)
    if not user:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect email or password",
            headers={"WWW-Authenticate": "Bearer"},
        )
    token = create_access_token(subject=user.id)
    log_audit(db, user.id, user.email, "LOGIN", "USER", str(user.id), {"role": user.role})
    return {
        "access_token": token,
        "token_type": "bearer",
        "role": user.role,
        "user_id": user.id,
        "email": user.email
    }

@router.post("/register", response_model=UserRead, status_code=status.HTTP_201_CREATED)
@router.post("/users", response_model=UserRead, status_code=status.HTTP_201_CREATED)
def register(user_in: UserRegister, db: Session = Depends(get_db), current_user: User = Depends(require_admin)):
    user = create_user(db, user_in)
    log_audit(db, current_user.id, current_user.email, "CREATE_USER", "USER", str(user.id), {"new_email": user.email, "role": user.role})
    return user

@router.get("/me", response_model=UserRead)
def read_current_user(current_user: User = Depends(get_current_user)):
    return current_user

@router.get("/users", response_model=List[UserRead])
def list_users(
    q: Optional[str] = Query(None, description="Search by name or email"),
    role: Optional[str] = Query(None, description="Filter by role: admin, operator, viewer"),
    is_active: Optional[bool] = Query(None, description="Filter by active status"),
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    query = db.query(User)
    if role:
        query = query.filter(User.role == role)
    if is_active is not None:
        query = query.filter(User.is_active == is_active)
    if q:
        search = f"%{q.strip()}%"
        query = query.filter((User.email.ilike(search)) | (User.full_name.ilike(search)))
    return query.order_by(User.id.asc()).all()

@router.get("/users/{user_id}", response_model=UserRead)
def get_user_by_id(user_id: int, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    if current_user.role != "admin" and current_user.id != user_id:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Access denied")
    user = db.query(User).filter(User.id == user_id).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    return user

@router.patch("/users/{user_id}", response_model=UserRead)
def update_user(user_id: int, user_in: UserUpdate, db: Session = Depends(get_db), current_user: User = Depends(require_admin)):
    user = db.query(User).filter(User.id == user_id).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")

    # Guard: Prevent demoting or deactivating the last active administrator
    will_demote_or_deactivate = False
    if user.role == "admin" and user.is_active:
        if user_in.is_active is False:
            will_demote_or_deactivate = True
        if user_in.role is not None and user_in.role != "admin":
            will_demote_or_deactivate = True

    if will_demote_or_deactivate:
        other_admins = db.query(User).filter(
            User.role == "admin",
            User.is_active == True,
            User.id != user_id
        ).count()
        if other_admins == 0:
            raise HTTPException(
                status_code=400,
                detail="Cannot demote or deactivate the last active administrator"
            )

    # Check email conflict if changing email
    if user_in.email and user_in.email != user.email:
        conflict = db.query(User).filter(User.email == user_in.email, User.id != user_id).first()
        if conflict:
            raise HTTPException(status_code=400, detail="A user with this email already exists")
        user.email = user_in.email

    if user_in.full_name is not None:
        user.full_name = user_in.full_name
    if user_in.role is not None:
        user.role = user_in.role
        user.is_superuser = (user_in.role == "admin")
    if user_in.is_active is not None:
        user.is_active = user_in.is_active
    if user_in.password:
        user.hashed_password = get_password_hash(user_in.password)

    db.commit()
    db.refresh(user)
    log_audit(db, current_user.id, current_user.email, "UPDATE_USER", "USER", str(user.id), {
        "updated_email": user.email,
        "role": user.role,
        "is_active": user.is_active
    })
    return user

@router.delete("/users/{user_id}", status_code=status.HTTP_200_OK)
def delete_user(
    user_id: int,
    hard_delete: bool = False,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    if current_user.id == user_id:
        raise HTTPException(status_code=400, detail="Cannot delete your own administrator account")

    user = db.query(User).filter(User.id == user_id).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")

    # Guard: Prevent deleting the last active administrator
    if user.role == "admin" and user.is_active:
        other_admins = db.query(User).filter(
            User.role == "admin",
            User.is_active == True,
            User.id != user_id
        ).count()
        if other_admins == 0:
            raise HTTPException(
                status_code=400,
                detail="Cannot delete the last active administrator"
            )

    if hard_delete:
        db.delete(user)
        action_msg = "HARD_DELETE_USER"
    else:
        # Default soft-delete / deactivate to preserve audit logs and relations
        user.is_active = False
        action_msg = "DEACTIVATE_USER"

    db.commit()
    log_audit(db, current_user.id, current_user.email, action_msg, "USER", str(user_id), {"email": user.email})
    return {"message": f"User {user_id} successfully {'deleted' if hard_delete else 'deactivated'}"}

