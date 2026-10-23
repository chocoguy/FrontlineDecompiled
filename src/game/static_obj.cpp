// Static objects: props built from a static mesh. Scripts can translate,
// rotate and scale them over time, play the mesh's frame animation or a
// motion clip, or send them along a spline path (trains, boats, trucks),
// optionally pacing the player. Moving ones can hurt what they hit.
#include "game/static_obj.h"

#include "game/staticmesh.h"
#include "game/staticobjfactory.h"
#include "game/objcreate.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/bullet.h"
#include "game/thrownbullet.h"
#include "game/light.h"
#include "game/scene.h"
#include "game/publisher.h"
#include "game/draw_context.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/BSGO_Basic.h"
#include "game/AIFilter.h"
#include "game/ShellMenu.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "math.h"

#define FLT_MAX 3.4028235e38f
#define TURN_DEADZONE 0.008726646f  // half a degree

// Script event 0xC0 (collision damage) data.
struct SCollisionDamageEvent {
    /* 0x00 */ BSObject* m_source;
    /* 0x04 */ int m_damage;
    /* 0x08 */ int field_0x8;
    /* 0x0C */ int field_0xC;
    /* 0x10 */ int field_0x10;
};

// Speeds a path turn rate up towards `diff`, or slows it in time to stop.
static inline void UpdateTurnRate(f32& rate, float diff, float accel, float max) {
    if (diff < -TURN_DEADZONE) {
        if (rate > 0.0f) {
            if ((float)(2.0 + fabs(rate / accel)) >= (float)fabs(diff / rate)) {
                rate = rate - accel;
            } else {
                rate = rate + accel;
                if (rate > max) {
                    rate = max;
                }
            }
        } else {
            rate = rate + accel;
            if (rate > max) {
                rate = max;
            }
        }
    } else if (diff > TURN_DEADZONE) {
        if (rate < 0.0f) {
            if ((float)(2.0 + fabs(rate / accel)) >= (float)fabs(diff / rate)) {
                rate = rate + accel;
            } else {
                rate = rate - accel;
                if (rate < -max) {
                    rate = -max;
                }
            }
        } else {
            rate = rate - accel;
            if (rate < -max) {
                rate = -max;
            }
        }
    } else {
        rate *= 0.6f;
    }
}

CStaticObject::CStaticObject() {
    ClearSceneNodeData();
    m_pNextFree = NULL;
    m_pPrevStatic = NULL;
    m_pNextStatic = NULL;
    m_pMesh = NULL;
    m_speed = 0.0f;
    m_targetSpeed = 0.0f;
    m_accel = 0.0f;
    m_pPaceTarget = NULL;
    m_paceDistance = 0.0f;
    m_pathState = PATH_NONE;
    m_pScriptObject = NULL;
    m_drawEnabled = 1;
    m_animReverse = 0;
    m_animEvent = (EBSEventEnum)-1;
    m_animStart = 0;
    m_animEnd = 0;
    m_numAnimFrames = 0;
    m_animFrameTime = 0;
    m_animFrame = 0;
    m_tm.Ident();
    m_soundPitchScale = 1.9979998f;
    m_pWorldBound = NULL;
    m_pWorldVolume = NULL;
    m_pLocalBound = NULL;
    m_pLocalVolume = NULL;
    m_rotating = 0;
    m_translating = 0;
    m_rotateTime = 0.0f;
    m_translateTime = 0.0f;
    m_collisionId = (u32)-1;
    m_pathState = PATH_NONE;
    m_collisionDamage = 0;
    m_turnSlowdown = 0.5f;
    m_pathNoTurn = 0;
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_flags = 0;
    m_isDumpObject = 0;
    m_scriptMotionBlocked = 0;
}

CStaticObject::CStaticObject(CStaticMesh* mesh, int numAnimFrames, int animFrameTime,
                             int numMotionFrames, int motionFrameTime, int motionRelative,
                             CStaticObject::MotionFrame* motion) {
    ClearSceneNodeData();
    m_pNextFree = NULL;
    m_pPrevStatic = NULL;
    m_pNextStatic = NULL;
    m_pMesh = mesh;
    m_pScriptObject = NULL;
    m_drawEnabled = 1;
    m_motionRelative = motionRelative != 0;
    m_motionPlaying = 0;
    m_animReverse = 0;
    m_animEvent = (EBSEventEnum)-1;
    m_animStart = 0;
    m_animEnd = 0;
    m_numAnimFrames = numAnimFrames;
    m_animTime = 0.0f;
    m_animFrameTime = animFrameTime;
    m_animFrame = 0;
    m_motionEvent = (EBSEventEnum)-1;
    m_motionStart = 0;
    m_motionEnd = 0;
    m_numMotionFrames = numMotionFrames;
    m_motionTime = 0.0f;
    m_motionFrameTime = motionFrameTime;
    m_motionFrame = 0;
    m_pMotion = motion;
    m_tm.Ident();
    m_soundPitchScale = 1.9979998f;
    m_pWorldBound = NULL;
    m_pWorldVolume = NULL;
    m_pLocalBound = NULL;
    m_pLocalVolume = NULL;
    m_rotating = 0;
    m_translating = 0;
    m_rotateTime = 0.0f;
    m_translateTime = 0.0f;
    m_collisionId = (u32)-1;
    m_pathState = PATH_NONE;
    m_collisionDamage = 0;
    m_turnSlowdown = 0.5f;
    m_pathNoTurn = 0;
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_flags = 0;
    m_isDumpObject = 0;
    m_scriptMotionBlocked = 0;
}

CStaticObject::~CStaticObject() {}

void CStaticObject::SetLocalBoundingVolume(ISceneNode::EVolumeType type, IVolume* volume) {
    switch ((int)type) {
    case 3:
        m_pLocalBound = volume;
        return;
    case 0:
        m_pLocalVolume = volume;
        return;
    }
}

// Like the original, the new volume is transformed onto itself.
void CStaticObject::SetWorldBoundingVolume(ISceneNode::EVolumeType type, IVolume* volume) {
    switch ((int)type) {
    case 3:
        if (volume != m_pWorldVolume) {
            volume->TransformedCopy(*volume, m_tm);
        }
        m_pWorldBound = volume;
        return;
    case 0:
        if (volume != m_pWorldBound) {
            volume->TransformedCopy(*volume, m_tm);
        }
        m_pWorldVolume = volume;
        if (GetCollisionId() == -1) {
            SetCollisionId((EClsnId)0);
        }
        return;
    }
}

const IVolume* CStaticObject::GetLocalBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pLocalBound;
    case 0:
    case 1:
    case 2:
        return m_pLocalVolume;
    default:
        return NULL;
    }
}

const IVolume* CStaticObject::GetWorldBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pWorldBound;
    case 0:
    case 1:
    case 2:
        return m_pWorldVolume;
    default:
        return NULL;
    }
}

int CStaticObject::Init() {
    m_rotation.SetFromMatrix(m_tm);
    m_scale = 1.0f;
    m_drawEnabled = 1;
    m_scaling = 0;
    m_translating = 0;
    m_rotating = 0;
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_pathState = PATH_NONE;
    m_collisionDamage = 0;
    m_turnSlowdown = 0.5f;
    m_pathNoTurn = 0;
    END_RETURN_NOTHING
}

// Steps the mesh's frame animation.
void CStaticObject::AnimUpdate(float dt) {
    if (m_numAnimFrames == 0 || m_animFrameTime == 0) {
        return;
    }
    m_animTime += dt;
    while (m_animTime >= m_animFrameTime) {
        m_animTime -= m_animFrameTime;
        if (m_animReverse) {
            m_animFrame--;
        } else {
            m_animFrame++;
        }
    }
    if (m_animEnd != 0) {
        if (m_animFrame >= m_animEnd && !m_animReverse) {
            if (m_animEvent != -1) {
                TriggerScriptEvent(m_animEvent, NULL, false);
                m_animEvent = (EBSEventEnum)-1;
            }
            m_animFrame = m_animEnd - 1;
            m_animFrameTime = 0;
        } else if (m_animFrame <= m_animEnd && m_animReverse) {
            if (m_animEvent != -1) {
                TriggerScriptEvent(m_animEvent, NULL, false);
                m_animEvent = (EBSEventEnum)-1;
            }
            m_animFrame = m_animEnd;
            m_animFrameTime = 0;
        }
    } else if (m_animReverse) {
        while (m_animFrame < 0) {
            m_animFrame += m_numAnimFrames;
        }
    } else {
        while (m_animFrame >= m_numAnimFrames) {
            m_animFrame -= m_numAnimFrames;
        }
    }
}

// Picks the motion clip pose for the end of this frame and the velocities
// that get there.
void CStaticObject::BeginAnimMotionUpdate(float dt) {
    if (m_numMotionFrames == 0 || m_motionFrameTime == 0) {
        return;
    }
    m_motionPos = *(const CVector3*)m_tm.m[3];
    m_motionRot.SetFromMatrix(m_tm);
    m_motionTime += dt;
    while (m_motionTime >= m_motionFrameTime) {
        m_motionTime -= m_motionFrameTime;
        m_motionFrame++;
    }

    int frame = m_motionFrame;
    int next = frame + 1;
    if (m_motionEnd != 0) {
        if (next >= m_motionEnd) {
            next = m_motionEnd - 1;
        }
        if (frame >= m_motionEnd) {
            if (m_motionEvent != -1) {
                TriggerScriptEvent(m_motionEvent, NULL, false);
                m_motionEvent = (EBSEventEnum)-1;
            }
            m_motionFrame = m_motionEnd - 1;
            m_motionPlaying = 0;
        }
    } else {
        m_motionFrame %= m_numMotionFrames;
        next = (frame + 1) % m_numMotionFrames;
    }

    float t = m_motionTime / m_motionFrameTime;
    CVector3 from = m_pMotion[m_motionFrame].m_pos;
    CVector3 to = m_pMotion[next].m_pos;
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    CVector3 pos;
    pos.x = t * (to.x - from.x) + from.x;
    pos.y = t * (to.y - from.y) + from.y;
    pos.z = t * (to.z - from.z) + from.z;
    CQuaternion fromRot = m_pMotion[m_motionFrame].m_rot;
    CQuaternion toRot = m_pMotion[next].m_rot;
    CQuaternion rot;
    rot.Slerp(toRot, fromRot, t);
    if (m_motionRelative) {
        m_motionBaseRot.Rotate(pos, pos);
        pos.x += m_motionBasePos.x;
        pos.y += m_motionBasePos.y;
        pos.z += m_motionBasePos.z;
        rot.Multiply(m_motionBaseRot);
    }

    float inv = 1.0f / dt;
    m_velocity.x = pos.x - m_motionPos.x;
    m_velocity.y = pos.y - m_motionPos.y;
    m_velocity.z = pos.z - m_motionPos.z;
    m_motionTargetRot = rot;
    m_velocity.x = m_velocity.x * inv;
    m_velocity.y = m_velocity.y * inv;
    m_velocity.z = m_velocity.z * inv;
    m_invStep = inv;
}

int CStaticObject::BeginUpdate(float dt) {
    m_lightVolumes.Update(dt);
    AnimUpdate(dt);
    if (IsPathMoving()) {
        BeginPathMotionUpdate(dt);
        RETURN_NOTHING;
    }
    if (IsScriptMoving()) {
        m_lastPos = *(const CVector3*)m_tm.m[3];
        if (m_translating && m_translateTime <= 0.0f) {
            TriggerScriptEvent(m_translateEvent, NULL, false);
            m_translateTime = 0.0f;
            m_translating = 0;
            m_velocity.z = 0.0f;
            m_velocity.y = 0.0f;
            m_velocity.x = 0.0f;
        }
        if (m_rotating) {
            m_rotation.Normalize();
            if (m_rotateTime <= 0.0f) {
                TriggerScriptEvent(m_rotateEvent, NULL, false);
                m_rotateTime = 0.0f;
                m_rotating = 0;
                m_angularVelocity.Set(0.0f, 0.0f, 0.0f, 1.0f);
            }
        }
        if (m_scaling && m_scaleTime <= 0.0f) {
            TriggerScriptEvent(m_scaleEvent, NULL, false);
            m_scaleTime = 0.0f;
            m_scaling = 0;
            m_scaleRate = 0.0f;
        }
        m_scriptMotionBlocked = 0;
        RETURN_NOTHING;
    }
    if (m_motionPlaying) {
        BeginAnimMotionUpdate(dt);
        RETURN_NOTHING;
    }
    if (m_pWorldVolume != NULL) {
        CVector3 min;
        CVector3 max;
        m_pWorldVolume->GetExtents(min, max);
        m_pMinX->m_value = min.x - CONTACT_EPSILON;
        m_pMinY->m_value = min.y - CONTACT_EPSILON;
        m_pMaxX->m_value = CONTACT_EPSILON + max.x;
        m_pMaxY->m_value = CONTACT_EPSILON + max.y;
    } else {
        CVector3 pos;
        GetPosition(pos);
        m_pMinX->m_value = pos.x - CONTACT_EPSILON;
        m_pMinY->m_value = pos.y - CONTACT_EPSILON;
        m_pMaxX->m_value = CONTACT_EPSILON + pos.x;
        m_pMaxY->m_value = CONTACT_EPSILON + pos.y;
    }
    END_RETURN_NOTHING
}

// Script motions: the translation is along the object's own axes.
void CStaticObject::AttemptScriptMotionUpdate(float dt) {
    float scale = m_scale;
    float scaleStep = dt;
    float translateStep = dt;
    float rotateStep = dt;
    CVector3 move;
    move.z = 0.0f;
    move.y = 0.0f;
    move.x = 0.0f;
    if (m_translating) {
        float step = m_translateTime;
        if (!(step < dt)) {
            step = dt;
        }
        translateStep = step;
        move.x = step * m_velocity.x;
        move.y = step * m_velocity.y;
        move.z = step * m_velocity.z;
    }
    if (m_rotating) {
        float step = m_rotateTime;
        if (!(step < dt)) {
            step = dt;
        }
        rotateStep = step;
        if (step < 1.0f) {
            CQuaternion identity;
            identity.w = 1.0f;
            identity.x = 0.0f;
            identity.y = 0.0f;
            identity.z = 0.0f;
            m_newRotation.Slerp(m_angularVelocity, identity, step);
        } else {
            m_newRotation = m_angularVelocity;
        }
        m_newRotation.Multiply(m_rotation);
    } else {
        m_newRotation = m_rotation;
    }
    if (m_scaling) {
        float step = m_scaleTime;
        if (!(step < dt)) {
            step = dt;
        }
        scaleStep = step;
        scale += m_scaleRate * step;
    }
    m_newRotation.GetMatrix(m_tm);
    m_tm.SetPos(m_lastPos);
    m_tm.TransformPoint(move, move);
    m_tm.SetPos(move);
    CMatrix scaling;
    scaling.BuildScale(scale);
    m_tm.Multiply(scaling, m_tm);
    m_translateStep = translateStep;
    m_rotateStep = rotateStep;
    m_scaleStep = scaleStep;
    UpdateWorldVolume();
}

void CStaticObject::AttemptAnimMotionUpdate(float dt) {
    if (m_numMotionFrames != 0 && m_motionFrameTime != 0) {
        CVector3 move;
        move.x = dt * m_velocity.x;
        move.y = dt * m_velocity.y;
        move.z = dt * m_velocity.z;
        CQuaternion rot;
        rot.Slerp(m_motionTargetRot, m_motionRot, dt * m_invStep);
        rot.GetMatrix(m_tm);
        m_tm.SetPos(m_motionPos);
        m_tm.Translate(move);
    }
    UpdateWorldVolume();
}

// Turns towards the next path point (yaw, then pitch) and moves forward.
void CStaticObject::AttemptPathMotionUpdate(float dt) {
    CVector3 dir;
    dir.x = m_pathDest.x - m_lastPos.x;
    dir.y = m_pathDest.y - m_lastPos.y;
    dir.z = m_pathDest.z - m_lastPos.z;
    dir.Normalize();

    if (!m_pathNoTurn) {
        CVector3 v = *(const CVector3*)m_tm.m[1];
        float yaw = MathFunAtan2F(v.x, v.y);
        MathFunRotateAboutZ(&v.x, &v.y, -yaw);
        float pitch = MathFunAtan2F(-v.z, v.y);
        v = dir;
        float wantYaw = MathFunAtan2F(v.x, v.y);
        MathFunRotateAboutZ(&v.x, &v.y, -wantYaw);
        float wantPitch = MathFunAtan2F(-v.z, v.y);

        UpdateTurnRate(m_yawRate, MathFunNormalizeAngleNegativePiToPi(wantYaw - yaw), m_turnAccel,
                       m_maxTurnRate);
        UpdateTurnRate(m_pitchRate, MathFunNormalizeAngleNegativePiToPi(wantPitch - pitch),
                       m_turnAccel, m_maxTurnRate);

        m_tm.RotateX(m_pitchRate);
        CVector3 right = *(const CVector3*)m_tm.m[0];
        CVector3 front = *(const CVector3*)m_tm.m[1];
        CVector3 up = *(const CVector3*)m_tm.m[2];
        MathFunRotateAboutZ(&right.x, &right.y, -m_yawRate);
        MathFunRotateAboutZ(&front.x, &front.y, -m_yawRate);
        MathFunRotateAboutZ(&up.x, &up.y, -m_yawRate);
        m_tm.SetRight(right);
        m_tm.SetFront(front);
        m_tm.SetUp(up);
        if (GetFrameCount() & 0x20) {
            m_tm.Orthonormalize();
        }
    }

    // Slower while turning hard.
    float target;
    if (m_turnSlowdown != 0.0f) {
        target = m_targetSpeed * (1.0 - (m_turnSlowdown / m_maxTurnRate) * fabs(m_yawRate));
    } else {
        target = m_targetSpeed;
    }
    float speed = MathFunRandomReal(0.8f * target, target);
    if (m_speed > speed) {
        m_speed = m_lastSpeed - 2.0f * m_accel * dt;
        if (m_speed < speed) {
            m_speed = speed;
        }
    } else {
        m_speed = m_accel * dt + m_lastSpeed;
        if (m_speed > speed) {
            m_speed = speed;
        }
    }

    if (m_pathNoTurn) {
        float d = m_speed * dt;
        dir.x *= d;
        dir.y *= d;
        dir.z *= d;
    } else {
        CVector3 front = *(const CVector3*)m_tm.m[1];
        float d = m_speed * dt;
        dir.x = d * front.x;
        dir.y = d * front.y;
        dir.z = d * front.z;
    }
    dir.x += m_lastPos.x;
    dir.y += m_lastPos.y;
    dir.z += m_lastPos.z;
    m_tm.SetPos(dir);
    UpdateWorldVolume();
}

int CStaticObject::AttemptUpdate(float dt) {
    if (IsPathMoving()) {
        AttemptPathMotionUpdate(dt);
        RETURN_NOTHING;
    }
    if (IsScriptMoving()) {
        if (!m_scriptMotionBlocked) {
            AttemptScriptMotionUpdate(dt);
        }
    } else if (m_motionPlaying) {
        AttemptAnimMotionUpdate(dt);
    }
    END_RETURN_NOTHING
}

void CStaticObject::CommitScriptMotionUpdate() {
    m_lastPos = *(const CVector3*)m_tm.m[3];
    if (m_translating) {
        float time = 0.0f;
        m_translateTime -= m_translateStep;
        if (!(m_translateTime < 0.0f)) {
            time = m_translateTime;
        }
        m_translateTime = time;
    }
    if (m_rotating) {
        float time = 0.0f;
        m_rotation = m_newRotation;
        m_rotateTime -= m_rotateStep;
        if (!(m_rotateTime < 0.0f)) {
            time = m_rotateTime;
        }
        m_rotateTime = time;
    }
    if (m_scaling) {
        float time = 0.0f;
        m_scale += m_scaleStep * m_scaleRate;
        m_scaleTime -= m_scaleStep;
        if (!(m_scaleTime < 0.0f)) {
            time = m_scaleTime;
        }
        m_scaleTime = time;
    }
}

void CStaticObject::CommitAnimMotionUpdate() {
    if (m_numMotionFrames != 0 && m_motionFrameTime != 0) {
        m_motionPos = *(const CVector3*)m_tm.m[3];
        m_motionRot.SetFromMatrix(m_tm);
    }
}

void CStaticObject::CommitPathMotionUpdate() {
    m_lastPos = *(const CVector3*)m_tm.m[3];
    m_lastSpeed = m_speed;
    UpdatePathMotionDestAndSpeed();
}

int CStaticObject::CommitUpdate() {
    if (m_pWorldBound != m_pWorldVolume) {
        m_pWorldBound->TransformedCopy(*m_pLocalVolume, m_tm);
    }
    if (IsPathMoving()) {
        CommitPathMotionUpdate();
    } else if (IsScriptMoving()) {
        CommitScriptMotionUpdate();
    } else if (m_motionPlaying) {
        CommitAnimMotionUpdate();
    }
    if (m_pLight != NULL) {
        CMatrix tm;
        GetTMLocalToWorld(tm);
        tm.PreTranslate(m_lightOffset);
        m_pLight->SetTMLocalToWorld(tm);
    }
    END_RETURN_NOTHING
}

void CStaticObject::Draw(CDrawContext& ctx) {
    if (m_drawEnabled) {
        ctx.m_pWorld = &m_tm;
        if ((u8)m_pMesh->IsLightingEnabled()) {
            CStaticMesh::SetLightVolume(m_lightVolumes.GetVolume());
        }
        m_pMesh->Draw(ctx, m_animFrame);
    }
}

// Collision damage (see AttemptCollisionDamage), then undoes this frame's
// motion: path movers stop unless they hit a bullet; script movers stay put
// unless the player is on the side the hit point moves away from.
void CStaticObject::OnCollision(const CCollision& clsn) {
    if (m_collisionDamage && (m_targetSpeed != 0.0f || m_speed != 0.0f)) {
        ISceneNode* other = clsn.m_pNode0;
        if (other == this) {
            other = clsn.m_pNode1;
        }
        SCollisionDamageEvent event;
        event.m_source = m_pScriptObject;
        event.m_damage = 1;
        event.field_0x10 = 9;
        event.field_0x8 = 1;
        event.field_0xC = 5;
        if (g_cheats.m_invincible && other->AsPlayerObject() != NULL) {
            event.m_damage = 0;
        }
        if (other->GetScriptObject() != NULL) {
            BSObjectTriggerEvent(other->GetScriptObject(), 0xC0, &event, NULL, true);
        }
    }

    if (IsPathMoving()) {
        m_translateStep = 0.0f;
        m_rotateStep = 0.0f;
        m_scaleStep = 0.0f;
        m_newRotation = m_rotation;
        ISceneNode* other = clsn.m_pNode0;
        if (other == this) {
            other = clsn.m_pNode1;
        }
        if (other->AsBullet() == NULL) {
            m_tm.SetPos(m_lastPos);
            m_lastSpeed = 0.0f;
            m_speed = 0.0f;
        }
    } else if (IsScriptMoving()) {
        ISceneNode* other = clsn.m_pNode0;
        if (other == this) {
            other = clsn.m_pNode1;
        }
        bool blocked = true;
        CPlayerObject* player = other->AsPlayerObject();
        if (player != NULL && clsn.m_pContacts != NULL && !m_scaling) {
            // Where the contact point was at the start of the frame.
            CMatrix m;
            CVector3 point = clsn.m_pContacts->m_point;
            CVector3 oldPoint;
            m.FastInverse(m_tm);
            m.TransformPoint(oldPoint, point);
            m_rotation.GetMatrix(m);
            m.SetPos(m_lastPos);
            m.TransformPoint(oldPoint, oldPoint);

            CVector3 playerPos;
            ((ISceneNode*)player)->GetPosition(playerPos);
            CVector3 toPoint;
            toPoint.x = point.x - playerPos.x;
            toPoint.y = point.y - playerPos.y;
            toPoint.z = point.z - playerPos.z;
            toPoint.Normalize();
            CVector3 motion;
            motion.x = point.x - oldPoint.x;
            motion.y = point.y - oldPoint.y;
            motion.z = point.z - oldPoint.z;
            motion.Normalize();
            if (toPoint.z * motion.z + (toPoint.x * motion.x + toPoint.y * motion.y) > 0.0f) {
                blocked = false;
            }
        }
        if (blocked) {
            m_scriptMotionBlocked = 1;
            m_translateStep = 0.0f;
            m_rotateStep = 0.0f;
            m_scaleStep = 0.0f;
            m_newRotation = m_rotation;
            m_rotation.GetMatrix(m_tm);
            m_tm.SetPos(m_lastPos);
            CMatrix scaling;
            scaling.BuildScale(m_scale);
            m_tm.Multiply(scaling, m_tm);
        }
    } else if (m_motionPlaying) {
        m_translateStep = 0.0f;
        m_rotateStep = 0.0f;
        m_scaleStep = 0.0f;
        m_newRotation = m_rotation;
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
        m_motionTargetRot.SetFromMatrix(m_tm);
    }
}

int CStaticObject::IsVisible(CDrawContext&) const {
    return 1;
}

int CStaticObject::GetCollisionId() const {
    return m_collisionId;
}

void CStaticObject::GetTMLocalToWorld(CMatrix& tm) const {
    tm = m_tm;
}

void CStaticObject::GetPosition(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[3];
}

void CStaticObject::GetRightward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[0];
}

void CStaticObject::GetForward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[1];
}

void CStaticObject::GetUpward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[2];
}

void CStaticObject::SetTMLocalToWorld(const CMatrix& tm) {
    m_tm = tm;
    m_rotation.SetFromMatrix(tm);
}

void CStaticObject::SetPosition(CVector3 pos) {
    m_tm.SetPos(pos);
}

void CStaticObject::SetBasis(CVector3 right, CVector3 front, CVector3 up) {
    m_tm.SetRight(right);
    m_tm.SetFront(front);
    m_tm.SetUp(up);
    m_rotation.SetFromMatrix(m_tm);
}

void CStaticObject::EnterLightVolume(BPDLightVolume* volume) {
    m_lightVolumes.AddVolume(volume);
}

void CStaticObject::ExitLightVolume(BPDLightVolume* volume) {
    m_lightVolumes.RemoveVolume(volume);
}

BPDLightVolume* CStaticObject::GetLightVolume() {
    return m_lightVolumes.GetVolume();
}

// A negative frame time with start > end plays backwards.
void CStaticObject::PlayAnimation(int frameTime, int start, int end, EBSEventEnum event) {
    if (frameTime < 0 && start > end) {
        frameTime = -frameTime;
        m_animReverse = 1;
    } else if (frameTime < 0) {
        frameTime = 0;
        m_animReverse = 0;
    } else {
        m_animReverse = 0;
    }
    if (start < 0) {
        start = 0;
    }
    if (start >= m_numAnimFrames) {
        start = m_numAnimFrames - 1;
    }
    if (end < 0) {
        end = 0;
    }
    if (end >= m_numAnimFrames) {
        end = m_numAnimFrames;
    }
    m_animFrameTime = frameTime;
    m_animStart = start;
    m_animFrame = start;
    m_animEnd = end;
    m_animEvent = event;
}

void CStaticObject::StartMotionPlayback(int start, int end, EBSEventEnum event) {
    if (m_pMotion == NULL) {
        return;
    }
    if (start < 0) {
        start = 0;
    }
    if (start >= m_numMotionFrames) {
        start = m_numMotionFrames - 1;
    }
    if (end < 0) {
        end = 0;
    }
    if (end >= m_numMotionFrames) {
        end = m_numMotionFrames;
    }
    m_motionStart = start;
    m_motionFrame = start;
    m_motionEnd = end;
    m_motionEvent = event;
    m_motionBaseRot.SetFromMatrix(m_tm);
    m_motionBasePos = *(const CVector3*)m_tm.m[3];
    if (!m_motionRelative) {
        m_motionRot = m_pMotion[m_motionFrame].m_rot;
        m_motionPos.Copy(m_pMotion[m_motionFrame].m_pos);
        m_motionRot.GetMatrix(m_tm);
        m_tm.SetPos(m_motionPos);
    }
    m_motionPlaying = 1;
}

void CStaticObject::StopMotionPlayback() {
    m_motionPlaying = 0;
}

// Moves `distance` at `speed` (per second, per axis); the motion ends at
// the first axis to arrive.
void CStaticObject::TranslateFromScript(CVector3& speed, CVector3& distance, EBSEventEnum event) {
    m_translating = 1;
    m_translateEvent = event;
    if (distance.x == 0.0f && distance.y == 0.0f && distance.z == 0.0f) {
        m_translateTime = FLT_MAX;
    } else {
        float tx = 0.0f;
        float ty = 0.0f;
        float tz = 0.0f;
        if (speed.x != 0.0f && distance.x != 0.0f) {
            tx = distance.x / speed.x;
        }
        if (speed.y != 0.0f && distance.y != 0.0f) {
            ty = distance.y / speed.y;
        }
        if (speed.z != 0.0f && distance.z != 0.0f) {
            tz = distance.z / speed.z;
        }
        bool keep = false;
        if ((tx < ty && tx > 0.0f) || ty == 0.0f) {
            keep = true;
        }
        if (!keep) {
            tx = ty;
        }
        keep = false;
        if ((tx < tz && tx > 0.0f) || tz == 0.0f) {
            keep = true;
        }
        if (!keep) {
            tx = tz;
        }
        m_translateTime = tx;
    }
    m_velocity = speed;
}

// Like TranslateFromScript with angles; both are negated first.
void CStaticObject::RotateFromScript(CVector3& speed, CVector3& angle, EBSEventEnum event) {
    CVector3 a = angle;
    CVector3 s = speed;
    float ax = -a.x;
    float ay = -a.y;
    float az = -a.z;
    m_rotating = 1;
    s.x = -s.x;
    s.y = -s.y;
    s.z = -s.z;
    a.x = ax;
    a.y = ay;
    a.z = az;
    m_rotateEvent = event;
    if (ax == 0.0f && ay == 0.0f && az == 0.0f) {
        m_rotateTime = FLT_MAX;
    } else {
        float tx = 0.0f;
        float ty = 0.0f;
        float tz = 0.0f;
        if (s.x != 0.0f && ax != 0.0f) {
            tx = ax / s.x;
        }
        if (s.y != 0.0f && a.y != 0.0f) {
            ty = a.y / s.y;
        }
        if (s.z != 0.0f && a.z != 0.0f) {
            tz = a.z / s.z;
        }
        bool keep = false;
        if ((tx < ty && tx > 0.0f) || ty == 0.0f) {
            keep = true;
        }
        if (!keep) {
            tx = ty;
        }
        keep = false;
        if ((tx < tz && tx > 0.0f) || tz == 0.0f) {
            keep = true;
        }
        if (!keep) {
            tx = tz;
        }
        m_rotateTime = tx;
    }
    CMatrix m;
    m.BuildRotX(s.x);
    m.RotateY(s.y);
    m.RotateZ(s.z);
    m_angularVelocity.SetFromMatrix(m);
}

void CStaticObject::ScaleFromScript(float rate, float amount, EBSEventEnum event) {
    m_scaling = 1;
    m_scaleEvent = event;
    m_scaleRate = rate;
    m_scaleTime = amount / rate;
}

// id > 0 starts (or re-tunes) a path; -1 pauses, -2 stops at once, other
// values stop gradually.
void CStaticObject::SetMovementPath(int id, bool reverse, bool pace, float speed, float accel,
                                    float maxTurnRate, float turnAccel, BSObject*,
                                    float paceDistance, bool keepSpeed) {
    if (id > 0) {
        CAISplinePath* path = g_aigAIFilterGlobalObject.GetSplinePath(id, reverse);
        if (path != m_path.m_path) {
            m_path.PrepareForTraversalForward(path, false, NULL);
            if (pace) {
                m_pacePath.PrepareForTraversalForward(path, false, NULL);
                m_pacePath.m_state = 3;
            }
        } else if (m_path.m_state == 0 || m_pathState == PATH_NONE) {
            return;
        }
        if (!keepSpeed) {
            m_speed = 0.0f;
        }
        m_maxSpeed = speed;
        m_targetSpeed = speed;
        m_accel = accel;
        m_yawRate = 0.0f;
        m_pitchRate = 0.0f;
        m_maxTurnRate = maxTurnRate;
        m_turnAccel = turnAccel;
        if (pace) {
            m_pathState = PATH_PACE_PLAYER;
            m_pPaceTarget = (ISceneNode*)g_scene.GetPlayer(0);
            m_paceDistance = paceDistance * paceDistance;
            return;
        }
        m_pathState = PATH_FOLLOW;
    } else if (id == -1) {
        if (m_pathState == PATH_NONE) {
            m_path.m_state = 0;
            return;
        }
        m_pathState = PATH_PAUSING;
    } else if (id == -2) {
        m_speed = 0.0f;
        m_targetSpeed = 0.0f;
        m_pathState = PATH_NONE;
        m_path.m_state = 0;
    } else {
        if (m_pathState == PATH_NONE) {
            m_path.m_state = 0;
            return;
        }
        m_pathState = PATH_STOPPING;
    }
}

// Weak copy (CThrownBullet's vtable) placed here by the original.
ISceneNode* CThrownBullet::GetFiredBy() const {
    return m_pFiredBy;
}

// Projectiles tell the script where they hit; grenades and shells explode
// (types 3 and 4 only on live tanks).
void CStaticObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    CProjectileBullet* projectile = bullet->AsProjectile();
    CThrownBullet* thrown = bullet->AsThrown();
    if (projectile != NULL) {
        if (m_pScriptObject == NULL) {
            return;
        }
        ISceneNode* firedBy = bullet->GetFiredBy();
        if (firedBy == this) {
            bullet->Penetrate(clsn);
            return;
        }
        if (m_flags & 1) {
            bullet->Penetrate(clsn);
        }
        BS_STRUCT_HitByBulletInfo_struct* event =
            (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
        event->m_bulletType = bullet->GetScriptBulletType();
        event->m_damage = bullet->GetDamage();
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
        event->m_pPoly = clsn.m_pContacts->m_pPoly;
        BSObjectTriggerEvent(m_pScriptObject, 0x2B, event, NULL, false);
    } else if (thrown != NULL) {
        if (thrown->GetFiredBy() == this) {
            thrown->Penetrate(clsn);
            return;
        }
        if ((thrown->m_type != 3 && thrown->m_type != 4) || AsHierTankObject() != NULL) {
            if (AsHierTankObject() == NULL ||
                *(int*)((ISceneNode*)AsHierTankObject())->GetScriptObject()->m_gameObject->GetScriptData() > 0) {
                thrown->Explode();
            }
        }
    }
}

void CStaticObject::SetAttachedLight(CLight* light, CVector3 offset) {
    if (light != m_pLight) {
        CPublisher::CancelSubscription(*this, (ISubject**)&m_pLight);
        m_pLight = light;
        if (light != NULL) {
            CPublisher::AddSubscription(*this, (ISubject**)&m_pLight, SECB_NullifyPointerOnMarked);
        }
    }
    if (m_pLight != NULL) {
        m_lightOffset = offset;
        CMatrix tm;
        GetTMLocalToWorld(tm);
        tm.PreTranslate(m_lightOffset);
        m_pLight->SetTMLocalToWorld(tm);
    }
}

// Aims at the next path point and picks the target speed. Pacing objects
// keep about m_paceDistance ahead of the player along the path.
void CStaticObject::UpdatePathMotionDestAndSpeed() {
    CAIFilterRealPosition pos;
    CVector3 last = m_lastPos;
    pos.field_0x0 = last.x;
    pos.field_0x4 = last.y;
    pos.field_0x8 = last.z;
    CAIFilterRealPosition next;
    if (m_pathState != PATH_ARRIVING) {
        if ((m_pathState >= PATH_ARRIVING || m_pathState < PATH_FOLLOW) && m_speed <= 0.0f) {
            if (m_pathState == PATH_PAUSING) {
                m_pathState = PATH_PAUSED;
            } else {
                m_pathState = PATH_NONE;
            }
            m_speed = 0.0f;
            m_targetSpeed = 0.0f;
            m_translating = 0;
        }
        m_path.GetNextPointForward(pos, &next, NULL);
        CAIFilterRealPosition end;
        m_path.GetFinalPointForward(&end);
        float distSq = pos.GetDistanceSquaredXYZReal(end);
        if (distSq < 400.0f) {
            float stopTime = m_speed / (2.0f * m_accel);
            float stopDist = 0.001 * (stopTime * stopTime);
            if (m_path.m_state == SPLINE_DONE || distSq < stopDist * stopDist) {
                m_pathState = PATH_ARRIVING;
                pos.GetDistanceSquaredXYZReal(next);
                m_path.GetFinalPointForward(&next);
            }
        }
        m_pathDest.Copy(*(const CVector3*)&next);
    } else {
        m_path.GetFinalPointForward(&next);
        if (pos.GetDistanceSquaredXYZReal(next) < 4.0f) {
            TriggerScriptEvent(0x37, NULL, false);
            m_speed = 0.0f;
            m_targetSpeed = 0.0f;
            m_path.m_path = NULL;
            m_pathState = PATH_NONE;
            m_translating = 0;
        }
        m_pathDest.Copy(*(const CVector3*)&next);
    }

    switch (m_pathState) {
    case PATH_PACE_PLAYER: {
        CVector3 targetPos;
        m_pPaceTarget->GetPosition(targetPos);
        CVector3 dir;
        dir.x = next.field_0x0 - pos.field_0x0;
        dir.y = next.field_0x4 - pos.field_0x4;
        dir.z = next.field_0x8 - pos.field_0x8;
        float along = dir.z * (targetPos.z - pos.field_0x8) +
                      (dir.x * (targetPos.x - pos.field_0x0) + dir.y * (targetPos.y - pos.field_0x4));
        dir.Normalize();
        CAIFilterRealPosition proj;
        proj.field_0x0 = pos.field_0x0 + dir.x * along;
        proj.field_0x4 = pos.field_0x4 + dir.y * along;
        proj.field_0x8 = pos.field_0x8 + dir.z * along;
        float distSq = proj.GetDistanceSquaredXYZReal(pos);
        float ahead;
        if (along > 0.0f) {
            ahead = distSq + m_paceDistance;
        } else {
            ahead = -distSq + m_paceDistance;
        }
        if (g_Shell.Get_currentMission() == 4 && g_Shell.Get_currentStage() == 2 &&
            !g_bInMultiplayerMode) {
            if (ahead > 25.0f) {
                m_targetSpeed = m_maxSpeed;
            } else if (ahead < -25.0f) {
                m_targetSpeed = -0.6f * m_maxSpeed;
            } else {
                m_targetSpeed = 0.2f * m_maxSpeed;
            }
        } else if (ahead > 25.0f) {
            m_targetSpeed = m_maxSpeed;
        } else if (ahead < -25.0f) {
            m_targetSpeed = 0.0f;
        } else {
            m_targetSpeed = 0.6f * m_maxSpeed;
        }
        break;
    }
    case PATH_ARRIVING:
        m_targetSpeed = 0.0100100115f;
        break;
    case PATH_STOPPING:
    case PATH_PAUSING:
        m_targetSpeed = 0.0f;
        break;
    }
}

void CStaticObject::BeginPathMotionUpdate(float) {
    m_lastPos = *(const CVector3*)m_tm.m[3];
    m_lastSpeed = m_speed;
    m_translating = 1;
    UpdatePathMotionDestAndSpeed();
}

// Sends script event 0xC0 (hit by a moving object) to what this ran into.
void CStaticObject::AttemptCollisionDamage(const CCollision& clsn) {
    if (m_collisionDamage && (m_targetSpeed != 0.0f || m_speed != 0.0f)) {
        ISceneNode* other = clsn.m_pNode0;
        if (other == this) {
            other = clsn.m_pNode1;
        }
        SCollisionDamageEvent event;
        event.m_source = m_pScriptObject;
        event.m_damage = 1;
        event.field_0x10 = 9;
        event.field_0x8 = 1;
        event.field_0xC = 5;
        if (g_cheats.m_invincible && other->AsPlayerObject() != NULL) {
            event.m_damage = 0;
        }
        if (other->GetScriptObject() != NULL) {
            BSObjectTriggerEvent(other->GetScriptObject(), 0xC0, &event, NULL, true);
        }
    }
}

void CStaticObject::MarkForDestruction(int flags) {
    ISubject::MarkForDestruction(flags);
    g_scene.Remove(*this);
    if (m_pScriptObject != NULL) {
        m_pScriptObject->m_uniqueID = -1;
    }
}

void CStaticObject::Destroy() {
    if (m_pScriptObject != NULL && m_pScriptObject->m_gameObject != NULL) {
        m_pScriptObject->m_gameObject->Destroy();
        return;
    }
    if (m_isDumpObject) {
        DestroyDumpObject(this);
        return;
    }
    CStaticObjectFactory::DeallocateStaticObject(this);
}
