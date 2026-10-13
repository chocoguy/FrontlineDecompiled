// Script-placed collision volumes: a volume with a matrix that tells its
// script object where bullets hit it (front, back, left or right).
#include "game/collisionvolume.h"

#include "game/IVolume.h"
#include "game/bullet.h"
#include "game/scene.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/BSGO_Basic.h"

CCollisionVolume::CCollisionVolume() {
    m_pVolume = NULL;
}

CCollisionVolume::~CCollisionVolume() {}

void CCollisionVolume::Init(IVolume* volume, CMatrix* tm, EClsnId id) {
    m_pVolume = volume;
    m_tm = *tm;
    m_collisionId = id;
}

void CCollisionVolume::GetTMLocalToWorld(CMatrix& tm) const {
    tm = m_tm;
}

void CCollisionVolume::GetPosition(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[3];
}

void CCollisionVolume::GetRightward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[0];
}

void CCollisionVolume::GetForward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[1];
}

void CCollisionVolume::GetUpward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[2];
}

const IVolume* CCollisionVolume::GetWorldBoundingVolume(ISceneNode::EVolumeType) const {
    return m_pVolume;
}

const IVolume* CCollisionVolume::GetLocalBoundingVolume(ISceneNode::EVolumeType) const {
    return m_pVolume;
}

int CCollisionVolume::BeginUpdate(float) {
    if (m_pVolume != NULL) {
        CVector3 min;
        CVector3 max;
        m_pVolume->GetExtents(min, max);
        SetExtents(min, max);
    } else {
        CVector3 pos;
        GetPosition(pos);
        SetExtents(pos, pos);
    }
}

void CCollisionVolume::HandleBulletCollision(CBullet* bullet, const CCollision&) {
    if (bullet->AsProjectile() == NULL) {
        return;
    }
    BS_STRUCT_HitByBulletInfo_struct* event =
        (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
    event->m_bulletType = bullet->GetScriptBulletType();
    event->m_damage = bullet->GetDamage();
    ISceneNode* firedBy = bullet->GetFiredBy();
    if (firedBy != NULL) {
        event->m_firedBy = firedBy->GetScriptObject();
    } else {
        event->m_firedBy = NULL;
    }

    CVector3 dir;
    CVector3 forward;
    CVector3 right;
    bullet->GetForward(dir);
    GetForward(forward);
    GetRightward(right);
    float alongRight = right.z * dir.z + (right.x * dir.x + right.y * dir.y);
    float alongForward = forward.z * dir.z + (forward.x * dir.x + forward.y * dir.y);
    if (alongRight < -0.7071f) {
        event->m_side = 3;
    } else if (alongRight > 0.7071f) {
        event->m_side = 2;
    } else if (alongForward < 0.0f) {
        event->m_side = 0;
    } else {
        event->m_side = 1;
    }
    BSObjectTriggerEvent(m_pScriptObject, 0x2B, event, NULL, false);
}

void CCollisionVolume::MarkForDestruction(int flags) {
    ISubject::MarkForDestruction(flags);
    g_scene.Remove(*this);
}

void CCollisionVolume::Destroy() {
    if (m_pScriptObject != NULL && m_pScriptObject->m_gameObject != NULL) {
        m_pScriptObject->m_gameObject->Destroy();
    }
}

void CCollisionVolume::SetCollisionId(EClsnId id) {
    m_collisionId = id;
}

BSObject* CCollisionVolume::GetScriptObject() const {
    return m_pScriptObject;
}

CCollisionVolume* CCollisionVolume::AsCollisionVolume() {
    return this;
}

const CCollisionVolume* CCollisionVolume::AsCollisionVolume() const {
    return this;
}

int CCollisionVolume::GetCollisionId() const {
    return m_collisionId;
}
