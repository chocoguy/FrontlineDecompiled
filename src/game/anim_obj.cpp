// Animated objects: the base of soldiers, the player's weapon and the
// animated player. The skinned animation drives the root bone; each frame
// the object turns by the animation's rotation and moves by its velocity
// (from the animation's root translation, gravity or an applied force),
// collects contact normals during collision, resolves them against its
// velocity, and snaps to the ground.
#include "game/anim_obj.h"

#include "game/animated_volume.h"
#include "game/fiber.h"
#include "game/line.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/bullet.h"
#include "game/thrownbullet.h"
#include "game/camera.h"
#include "game/scene.h"
#include "game/world_object.h"
#include "game/sound.h"
#include "game/surfacetype.h"
#include "game/draw_context.h"
#include "game/gameconfig.h"
#include "game/ShellMenu.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/shadow.h"
#include "math.h"

#define ANGLE_TO_UNITS 2670176.8f  // 2^24 / 2pi
#define TWO_PI 6.2831855f
#define ANGLE_UNITS 1.6777216e7f
#define GRAVITY 0.005455351f
#define MAX_FALL_SPEED 1.5f

// Bullet base implementations; weak copies placed here by the original
// (CBullet's vtable needs them).
void CBullet::GetForward(CVector3& v) const {
    v = m_forward;
}

ISceneNode* CBullet::GetFiredBy() const {
    return NULL;
}

float CBullet::GetDamage() const {
    return 0.0f;
}

int CBullet::GetScriptBulletType() const {
    return -1;
}

CThrownBullet* CBullet::AsThrown() {
    return NULL;
}

CProjectileBullet* CBullet::AsProjectile() {
    return NULL;
}

// CMatrix's array default constructor; a weak copy the original placed here
// that other objects (Soldier_object, dmcluster) link against.
extern "C" CMatrix* __defctor__7CMatrixFv(CMatrix* m) {
    if (!CMatrix::s_ClassInit) {
        CMatrix::InitClass();
    }
    return m;
}

void CAnimObject::InitClass(CRenderList*, CScreen*) {
    CAnimated::InitClass();
    CCharacterShadow::InitClass();
}

CAnimObject::CAnimObject() : m_contacts(-1) {
    ClearSceneNodeData();
    m_pHitVolume = NULL;
}

int CAnimObject::Init() {
    m_anim.Init(this, 0, NULL);
    m_pPrevAnim = NULL;
    m_pNextAnim = NULL;
    m_pNextFree = NULL;
    m_tm.Ident();
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_turnFrom[2] = 0.0f;
    m_turnFrom[1] = 0.0f;
    m_turnFrom[0] = 0.0f;
    m_turnMove[2] = 0.0f;
    m_turnMove[1] = 0.0f;
    m_turnMove[0] = 0.0f;
    m_heading = 0;
    for (int i = 0; i < NUM_BONES; i++) {
        m_bones[i].Ident();
    }
    m_pLocalBound = NULL;
    m_pLocalVolume = NULL;
    m_pWorldBound = NULL;
    m_pWorldVolume = NULL;
    m_collisionId = (u32)-1;
    m_contacts.reserve(m_contactBuffer, MAX_CONTACTS);
    m_pScriptObject = NULL;
    m_attached = 0;
    m_attachedToLocation = 0;
    field_0x2459 = 0;
    m_pathTurn = 0;
    field_0x2452 = 1;
    m_animEndReported = 0;
    field_0x24B2 = 1;
    field_0x24B3 = 1;
    m_falling = 0;
    m_forced = 0;
    field_0x2454 = 1;
    m_animQueued = 0;
    field_0x24B0 = 0;
    field_0x2451 = 0;
    m_footstepSound = 1;
    m_noGround = 0;
    field_0x30D8 = 0.0f;
}

void CAnimObject::BindLocationCollision(CObjLocationCollisionData* data, CObjLocationCollisionMap* map) {
    if (m_pHitVolume != NULL) {
        m_pHitVolume->Set(data, map);
    }
}

CAnimObject::~CAnimObject() {
    if (m_pHitVolume != NULL) {
        delete m_pHitVolume;
        m_pHitVolume = NULL;
    }
}

// The animation system may defer the state; then it is queued and its
// replacement started at once.
u32 CAnimObject::SetAnimationState(unsigned short state, float blend, long event, void* data) {
    unsigned short next;
    u32 result = m_anim.SetAnimationState(state, blend, next, event, data);
    if (result == 0xF0F0F0F0) {
        m_queuedState = state;
        m_queuedBlend = blend;
        m_queuedEvent = event;
        m_queuedData = data;
        m_animQueued = 1;
        result = m_anim.SetAnimationState(next, blend, next, event, data);
    } else {
        m_animQueued = 0;
    }
    m_animEndReported = 0;
    return result;
}

void CAnimObject::QueueAnimationState(unsigned short state, float blend, long event, void* data) {
    m_queuedState = state;
    m_queuedBlend = blend;
    m_queuedEvent = event;
    m_queuedData = data;
    m_animQueued = 1;
}

// An animation ended: start the queued one, or tell the script (once
// unless `again`).
int CAnimObject::FinishAnimationState(long event, bool again) {
    if (field_0x2452 && m_animEndReported) {
        return 0;
    }
    if (m_animQueued && !again) {
        m_animQueued = 0;
        field_0x24B0 = 1;
        SetAnimationState(m_queuedState, m_queuedBlend, m_queuedEvent, m_queuedData);
        field_0x2452 = 1;
    } else {
        field_0x24B0 = 0;
        BSObjectTriggerEvent(m_pScriptObject, (unsigned short)event, NULL, NULL, false);
        if (!again) {
            m_animEndReported = 1;
        }
    }
    return 0;
}

// Takes this frame's rotation and translation from the animation.
void CAnimObject::UpdateAnimation(float dt) {
    m_yawDelta = 0.0f;
    m_facingTarget = 0;
    m_turnMove[2] = 0.0f;
    m_turnMove[1] = 0.0f;
    m_turnMove[0] = 0.0f;
    m_turnLeft = 1.0f;
    m_anim.UpdateAnimation(dt);
    long rot = 0;
    if (!m_facingTarget && m_anim.ExtractRotation(&rot)) {
        float angle = (TWO_PI * rot) / ANGLE_UNITS;
        if (angle > 3.1415927f) {
            angle -= TWO_PI;
        } else if (angle < -3.1415927f) {
            angle += TWO_PI;
        }
        m_yawDelta += angle;
    }
    CVector3 trans;
    bool translate = false;
    if (m_anim.ExtractTranslation(&trans) && field_0x2459) {
        translate = true;
    }
    field_0x24B4 = 0;
    float inv = 1.0f / dt;
    if (!m_forced && !m_noGround) {
        if (translate) {
            m_velocity.x = inv * (m_turnLeft * trans.x + m_turnMove[0]);
            m_velocity.y = inv * (m_turnLeft * trans.y + m_turnMove[1]);
            if (!field_0x24B2) {
                m_velocity.z = inv * trans.z;
            }
            field_0x24B4 = 1;
        } else {
            m_velocity.x = 0.0f;
            m_velocity.y = 0.0f;
            if (!field_0x24B2) {
                m_velocity.z = 0.0f;
            }
        }
    } else if (m_noGround) {
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
    }
    m_yawDelta *= inv;
    if (m_pHitVolume != NULL) {
        m_pHitVolume->m_boxesValid = false;
    }
}

void CAnimObject::SetMoveTarget(const CVector3& target) {
    m_goal[0] = target.x;
    m_goal[1] = target.y;
    m_goal[2] = target.z;
    m_goal[3] = target.w;
    m_pathTurn = 1;
}

void CAnimObject::SetNodeState(long node, long state) {
    m_anim.SetGeomNodeState(node, state);
}

void CAnimObject::Draw(CDrawContext& ctx) {
    if (!g_config.field_0x3) {
        ctx.m_pWorld = &m_tm;
        CVector3 min;
        CVector3 max;
        m_anim.UpdateSkeletonMatrices(m_bones, &min, &max);
        m_anim.Draw(ctx);
        UpdateLocalBoundingVolume((ISceneNode::EVolumeType)3, min, max);
    }
}

void CAnimObject::SetLocalBoundingVolume(ISceneNode::EVolumeType type, IVolume* volume) {
    switch ((int)type) {
    case 3:
        m_pLocalBound = volume;
        return;
    case 0:
        m_pLocalVolume = volume;
        return;
    }
}

void CAnimObject::SetWorldBoundingVolume(ISceneNode::EVolumeType type, IVolume* volume) {
    switch ((int)type) {
    case 3:
        m_pWorldBound = volume;
        return;
    case 0:
        m_pWorldVolume = volume;
        return;
    }
}

const IVolume* CAnimObject::GetLocalBoundingVolume(ISceneNode::EVolumeType type) const {
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

const IVolume* CAnimObject::GetWorldBoundingVolume(ISceneNode::EVolumeType type) const {
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

void CAnimObject::TriggerScriptEvent(int event, void* data, bool immediate) {
    if (m_pScriptObject != NULL) {
        BSObjectTriggerEvent(m_pScriptObject, (unsigned short)event, data, NULL, immediate);
    }
}

// Projectiles report the side and location hit to the script (event 0x2B)
// with the damage scaled by location: x3 head and neck (0, 1), x0.5 limbs
// (3..8, 13..20), unchanged otherwise. Hits by the player count for the
// mission stats. Grenades explode.
void CAnimObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    CProjectileBullet* projectile = bullet->AsProjectile();
    CThrownBullet* thrown = bullet->AsThrown();
    if (projectile == NULL) {
        if (thrown != NULL) {
            thrown->Explode();
        }
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
    const SClsnContact* contact = clsn.m_pContacts;
    event->m_pPoly = contact->m_pPoly;
    int location = (int)event->m_pPoly;
    if (location < 9) {
        if (location == 2) {
            event->m_damage = (float)event->m_damage;
        } else if (location < 2) {
            if (location >= 0) {
                event->m_damage = 3.0f * event->m_damage;
            }
        } else {
            event->m_damage = 0.5f * event->m_damage;
        }
    } else if (location < 21) {
        if (location < 13) {
            event->m_damage = (float)event->m_damage;
        } else {
            event->m_damage = 0.5f * event->m_damage;
        }
    }
    BSObjectTriggerEvent(m_pScriptObject, 0x2B, event, NULL, false);
    // Like the original, assumes projectiles always have a shooter.
    if (firedBy->AsPlayerObject() != NULL) {
        g_Shell.Set_hits(1, false);
        switch ((int)event->m_pPoly) {
        case 3:
        case 4:
        case 5:
            g_Shell.Set_shotLocation(1, (EShotLocations)1, false);
            break;
        case 6:
        case 7:
        case 8:
            g_Shell.Set_shotLocation(1, (EShotLocations)2, false);
            break;
        case 13:
        case 14:
        case 15:
            g_Shell.Set_shotLocation(1, (EShotLocations)4, false);
            break;
        case 16:
        case 17:
        case 18:
            g_Shell.Set_shotLocation(1, (EShotLocations)5, false);
            break;
        case 19:
        case 20:
            g_Shell.Set_shotLocation(1, (EShotLocations)6, false);
            break;
        case 0:
        case 1:
            g_Shell.Set_shotLocation(1, (EShotLocations)0, false);
            break;
        case 2:
        case 9:
        case 10:
        case 11:
        case 12:
            g_Shell.Set_shotLocation(1, (EShotLocations)3, false);
            break;
        }
    }
    PlayImpactSound(0xE, 1, contact->m_point, false);
}

// Casts a short line down through the feet; on a hit returns how far to
// lift the object onto the ground. With nothing within 100 units either
// way the object is in the void and stops.
float CAnimObject::DetectGround() {
    float lift = 0.0f;
    CCollision clsn;
    int result = CLSN_NONE;
    m_noGround = 0;
    if (field_0x24B3 && (!m_forced || m_velocity.z < 0.0f)) {
        const IVolume* world =
            g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
        CVector3 pos = *(const CVector3*)m_tm.m[3];
        float feet = pos.z - 0.05f;
        CVector3 bottom(pos.x, pos.y, feet);
        CVector3 top(pos.x, pos.y, 1.0f + feet);
        CLine3 line(top, bottom);
        CVolFiber fiber(line);
        result = world->TestCollision(fiber, clsn, true);
        if (result == CLSN_NONE) {
            CVector3 low(pos.x, pos.y, feet - 100.0f);
            CVector3 high(pos.x, pos.y, 100.0f + feet);
            CLine3 longLine(high, low);
            CVolFiber longFiber(longLine);
            CCollision longClsn;
            if (world->TestCollision(longFiber, longClsn, true) == CLSN_NONE) {
                m_noGround = 1;
            }
        }
    }
    if (m_noGround) {
        m_velocity.x = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.z = 0.0f;
    } else if (result != CLSN_NONE) {
        field_0x2454 = 1;
        m_forced = 0;
        lift = 0.99f - clsn.m_fraction;
        if (m_falling) {
            TriggerScriptEvent(0x85, NULL, false);
            m_falling = 0;
        }
        m_footstepSound = 1;
        if (clsn.m_pContacts != NULL) {
            const SClsnPoly* poly = clsn.m_pContacts->m_pPoly;
            if (poly != NULL && poly->m_surfaceType >= 0) {
                m_footstepSound = CSurfaceType::GetSoundIndex(poly->m_surfaceType);
            }
        }
    }
    return lift;
}

// Remembers the start of the frame, runs the animation, then gravity.
int CAnimObject::BeginUpdate(float dt) {
    m_lightVolumes.Update(dt);
    CVector3 pos = *(const CVector3*)m_tm.m[3];
    m_turnFrom[0] = pos.x;
    m_turnFrom[1] = pos.y;
    m_turnFrom[2] = pos.z;
    m_turnFrom[3] = pos.w;
    m_lastHeading = m_heading;
    CMatrix root;
    root = m_bones[1];
    m_lastRootRot.SetFromMatrix(root);
    m_lastRootPos = *(const CVector3*)root.m[3];
    UpdateAnimation(dt);
    if (m_attached || m_noGround) {
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
    } else if (field_0x2454 && field_0x24B2) {
        m_velocity.z = 0.0f;
    } else if (field_0x24B2) {
        m_velocity.z = -GRAVITY * dt + m_velocity.z;
        float vz = m_velocity.z;
        if ((float)fabs(vz) > MAX_FALL_SPEED) {
            if (vz > 0.0f) {
                m_velocity.z = MAX_FALL_SPEED;
            } else {
                m_velocity.z = -MAX_FALL_SPEED;
            }
        }
        m_falling = 1;
    }
    m_constrained = 0;
}

// Turns and moves, interpolates the root bone, and updates the volumes.
int CAnimObject::AttemptUpdate(float dt) {
    float yaw = m_yawDelta * dt;
    CVector3 move;
    move.x = dt * m_velocity.x;
    move.y = dt * m_velocity.y;
    move.z = dt * m_velocity.z;
    CMatrix m;
    m = m_bones[1];
    CQuaternion rot;
    rot.SetFromMatrix(m);
    CVector3 rootPos = *(const CVector3*)m.m[3];
    m_rootPos.x = dt * (rootPos.x - m_lastRootPos.x) + m_lastRootPos.x;
    m_rootPos.y = dt * (rootPos.y - m_lastRootPos.y) + m_lastRootPos.y;
    m_rootPos.z = dt * (rootPos.z - m_lastRootPos.z) + m_lastRootPos.z;
    CQuaternion from = m_lastRootRot;
    CQuaternion to = rot;
    m_rootRot.Slerp(to, from, dt);
    m_rootRot.GetMatrix(m);
    m.SetPos(m_rootPos);
    if (m_tm.m[3][2] < -250.0f) {
        // Fell out of the world.
        BSObjectTriggerEvent(m_pScriptObject, 0x2D, NULL, NULL, false);
    }
    if (!m_attached) {
        m_heading = (m_lastHeading + ((int)(ANGLE_TO_UNITS * yaw) & 0xFFFFFF)) & 0xFFFFFF;
        m_tm.BuildRotZ((TWO_PI * m_heading) / ANGLE_UNITS);
        CVector3 from3(m_turnFrom[0], m_turnFrom[1], m_turnFrom[2]);
        from3.w = m_turnFrom[3];
        m_tm.SetPos(from3);
        m_tm.Translate(move);
        m_anim.SetRotation(m_heading);
    } else if (m_attachedToLocation) {
        m_tm = m_attachTM;
    }
    if (m_pLocalVolume != NULL) {
        m.Multiply(m, m_tm);
        ((CVolSphere*)m_pHitVolume)->SetCenter(CVector3(0.0f, 0.0f, 0.0f));
        m_pWorldVolume->TransformedCopy(*m_pLocalVolume, m);
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
    field_0x2454 = 0;
    m_contacts.m_size = 0;
    m_constrained = 0;
}

int CAnimObject::CommitUpdate() {
    ConstrainVelocity();
    CVector3 pos = *(const CVector3*)m_tm.m[3];
    m_turnFrom[0] = pos.x;
    m_turnFrom[1] = pos.y;
    m_turnFrom[2] = pos.z;
    m_turnFrom[3] = pos.w;
    m_lastHeading = m_heading;
    m_lastRootPos.Copy(m_rootPos);
    m_lastRootRot = m_rootRot;
    float lift = DetectGround();
    if (lift > 0.0f) {
        CVector3 up(0.0f, 0.0f, lift);
        m_tm.Translate(up);
        pos = *(const CVector3*)m_tm.m[3];
        m_turnFrom[0] = pos.x;
        m_turnFrom[1] = pos.y;
        m_turnFrom[2] = pos.z;
        m_turnFrom[3] = pos.w;
    }
}

// The velocity relative to a contact's mover, into its normal.
static inline float Approach(const CAnimObject* obj, const CAnimObject::CClsnInfo* c) {
    CVector3 rel = obj->m_velocity;
    if (c->m_pOther != NULL) {
        CVector3 v = c->m_pOther->GetWorldLinearVelocity();
        rel.x = rel.x - v.x;
        rel.y = rel.y - v.y;
        rel.z = rel.z - v.z;
    }
    return rel.z * c->m_normal.z + (rel.x * c->m_normal.x + rel.y * c->m_normal.y);
}

// Removes the velocity into the deepest contact; if that still runs into a
// second contact, solves for a velocity along both (crease); if a third
// still blocks, stops. Over 0.5 units a frame the result is dropped.
void CAnimObject::ConstrainVelocity() {
    if (m_contacts.m_size == 0) {
        return;
    }
    if (m_constrained) {
        return;
    }
    m_constrained = 1;
    float best1 = 1.0f;
    float best2 = 1.0f;
    CClsnInfo* begin = m_contacts.m_data;
    CClsnInfo* end = begin + m_contacts.m_size;
    CVector3 saved = m_velocity;
    CClsnInfo* first = end;
    CClsnInfo* second = end;
    for (CClsnInfo* c = begin; c != end; c++) {
        float d = Approach(this, c);
        if (d < 0.0f && d < best1) {
            best1 = d;
            first = c;
        }
    }
    bool blocked = first != end;
    float v1x = 0.0f;
    float v1y = 0.0f;
    float v1z = 0.0f;
    if (blocked) {
        v1x = best1 * first->m_normal.x;
        v1y = best1 * first->m_normal.y;
        v1z = best1 * first->m_normal.z;
        m_velocity.x = m_velocity.x - v1x;
        m_velocity.y = m_velocity.y - v1y;
        m_velocity.z = m_velocity.z - v1z;
        for (CClsnInfo* c = begin; c != end; c++) {
            if (c == first) {
                continue;
            }
            float d = Approach(this, c);
            if (d < -0.000001f && d < best2) {
                best2 = d;
                second = c;
            }
        }
        blocked = second != end;
    }
    if (blocked) {
        const CVector3& n1 = first->m_normal;
        const CVector3& n2 = second->m_normal;
        float t11 = best1 * (n1.z * n2.z + (n1.x * n2.x + n1.y * n2.y));
        float t12 = best2 + t11;
        float v2x = t12 * n2.x;
        float v2y = t12 * n2.y;
        float v2z = t12 * n2.z;
        float f8 = best1 * t12;
        float f52 = v2z * n1.z + (v2x * n1.x + v2y * n1.y);
        float det = f8 - f52 * t11;
        bool ok;
        float a;
        float b;
        if ((float)fabs(det) < 0.00001f) {
            ok = false;
        } else {
            ok = true;
            float inv = 1.0f / det;
            a = inv * (f8 - t11 * best1);
            b = inv * (t12 * best1 - f52 * t12);
        }
        if (ok) {
            m_velocity.x = saved.x - (b * v1x + v2x * a) * 1.05f;
            m_velocity.y = saved.y - (b * v1y + v2y * a) * 1.05f;
            m_velocity.z = saved.z - (b * v1z + v2z * a) * 1.05f;
        }
        for (CClsnInfo* c = begin; blocked && c != end; c++) {
            if (c != first && c != second) {
                if (m_velocity.z * c->m_normal.z +
                        (m_velocity.x * c->m_normal.x + m_velocity.y * c->m_normal.y) <
                    -0.000001f) {
                    blocked = false;
                    m_velocity.z = 0.0f;
                    m_velocity.y = 0.0f;
                    m_velocity.x = 0.0f;
                }
            }
        }
    }
    if (m_velocity.Length() > 0.5f) {
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
    }
}

// Collects the horizontal contact normals (and, when colliding with
// characters, the character so its motion counts).
void CAnimObject::OnCollision(const CCollision& clsn) {
    m_constrained = 0;
    bool first = clsn.m_pNode0 == this;
    ISceneNode* other = first ? clsn.m_pNode1 : clsn.m_pNode0;
    for (SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
        IMovingSceneNode* mover = NULL;
        if (field_0x2451) {
            int id = GetCollisionId();
            if ((id == 5 || id == 7) && (other->AsAnimObject() != NULL || other->AsPlayerObject() != NULL)) {
                mover = other->AsMovingNode();
            } else if (other->AsAnimObject() != NULL) {
                mover = other->AsMovingNode();
            }
        }
        CVector3 n;
        if (first) {
            n.x = -c->m_normal.x;
            n.y = -c->m_normal.y;
            n.z = -c->m_normal.z;
        } else {
            n = c->m_normal;
        }
        n.z = 0.0f;
        float lenSq = 0.0f * 0.0f + (n.x * n.x + n.y * n.y);
        if (lenSq > 0.001f) {
            n.Normalize();
            CClsnInfo& info = m_contacts.m_data[m_contacts.m_size++];
            info.m_normal.Copy(n);
            info.m_pOther = mover;
        }
    }
}

void CAnimObject::Halt() {
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_yawDelta = 0.0f;
}

// Pushes the object along `dir` (force / 512 units a frame).
void CAnimObject::ApplyForceTo(CVector3 dir, float force) {
    m_velocity.Copy(dir);
    float scale = force * 0.001953125f;
    m_velocity.Normalize();
    m_velocity.x = m_velocity.x * scale;
    m_velocity.y = m_velocity.y * scale;
    m_velocity.z = m_velocity.z * scale;
    m_forced = 1;
    field_0x2454 = 0;
}

void CAnimObject::GetTMLocalToWorld(CMatrix& tm) const {
    tm = m_tm;
}

int CAnimObject::IsVisible(CDrawContext& ctx) const {
    if (m_pLocalBound != NULL) {
        m_pWorldBound->TransformedCopy(*m_pLocalBound, m_tm);
    }
    if (m_pWorldBound != NULL) {
        return m_pWorldBound->TestVisibility(ctx);
    }
    CVector3 pos;
    GetPosition(pos);
    return ((CCamera*)ctx.m_pCamera)->IsPointVisible(pos, ctx);
}

int CAnimObject::IsDrawEnabled() const {
    return 1;
}

int CAnimObject::GetCollisionId() const {
    return m_collisionId;
}

void CAnimObject::Reset() {
    m_tm.Ident();
    m_heading = 0;
    field_0x2454 = 0;
}

void CAnimObject::SetPosition(CVector3 pos) {
    m_tm.SetPos(pos);
}

void CAnimObject::Move(CVector3 delta) {
    m_tm.Translate(delta);
}

int CAnimObject::Yaw(float angle) {
    m_tm.RotateZ(angle);
    m_heading = (m_heading + (int)(ANGLE_TO_UNITS * angle)) & 0xFFFFFF;
}

float CAnimObject::GetYaw() const {
    return (TWO_PI * m_heading) / ANGLE_UNITS;
}

void CAnimObject::SetYaw(float angle) {
    m_heading = (int)(ANGLE_TO_UNITS * angle) & 0xFFFFFF;
    m_lastHeading = m_heading;
}

void CAnimObject::Orthonormalize() {
    m_tm.Orthonormalize();
}

void CAnimObject::GetPosition(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[3];
}

void CAnimObject::GetRightward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[0];
}

void CAnimObject::GetForward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[1];
}

void CAnimObject::GetUpward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[2];
}

void CAnimObject::EnterLightVolume(BPDLightVolume* volume) {
    m_lightVolumes.AddVolume(volume);
}

void CAnimObject::ExitLightVolume(BPDLightVolume* volume) {
    m_lightVolumes.RemoveVolume(volume);
}

BPDLightVolume* CAnimObject::GetLightVolume() {
    return m_lightVolumes.GetVolume();
}

void CAnimObject::SetTMLocalToWorld(const CMatrix& tm) {
    m_tm = tm;
}

void CAnimObject::AttachToLocation(const CMatrix& tm) {
    m_attached = 1;
    m_attachedToLocation = 1;
    m_attachTM = tm;
}

void CAnimObject::SetAttached(bool attached) {
    m_attached = attached;
    m_attachedToLocation = 0;
}

void CAnimObject::MarkForDestruction(int flags) {
    ISubject::MarkForDestruction(flags);
    g_scene.Remove(*this);
}

void CAnimObject::Destroy() {}
