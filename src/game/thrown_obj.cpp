// Thrown objects: props that can be thrown, knocked about by bullets and
// roll (bottles, helmets, script-thrown items). Simple sphere physics with
// gravity, bounce and friction from the object's level-file resource.
#include "game/thrown_obj.h"

#include "game/staticmesh.h"
#include "game/objcreate.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/sphere.h"
#include "game/box.h"
#include "game/bullet.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/surfacetype.h"
#include "game/proxtrig.h"
#include "game/draw_context.h"
#include "game/bsobject.h"

#define GRAVITY 0.005455351f  // per frame squared

// A property value from the object's resource fields.
static inline float PropF(const CThrownObject* obj, int index) {
    return *(const f32*)&obj->m_pResource->fields[index].value;
}

static inline int PropI(const CThrownObject* obj, int index) {
    return obj->m_pResource->fields[index].value;
}

// Keeps the object from moving more than its diameter in one frame.
static inline void ClampSpeed(CThrownObject* obj) {
    if (obj->m_collisionId != (u32)-1) {
        float speed = obj->m_velocity.Length();
        float max = 2.0f * ((CVolSphere*)obj->m_pLocalVolume)->GetRadius();
        if (max < speed) {
            float scale = max / speed;
            obj->m_velocity.x = obj->m_velocity.x * scale;
            obj->m_velocity.y = obj->m_velocity.y * scale;
            obj->m_velocity.z = obj->m_velocity.z * scale;
        }
    }
}

// The impact sound for landing on the world, by surface.
static inline void PickImpactSound(CThrownObject* obj, ISceneNode* other, const SClsnContact* c) {
    if (obj->m_impactSound == 0 && other->AsWorldObject() != NULL) {
        if (c->m_pPoly != NULL && c->m_pPoly->m_surfaceType >= 0) {
            obj->m_impactSound = CSurfaceType::GetSoundIndex(c->m_pPoly->m_surfaceType);
        } else {
            obj->m_impactSound = 1;
        }
    }
}

CThrownObject::CThrownObject() {
    m_bsgo.m_object = NULL;
    m_bsgo.m_pNext = NULL;
    m_bsgo.m_pPrev = NULL;
    m_deleted = 0;
    m_marked = 0;
}

CThrownObject::CThrownObject(CStaticMesh* mesh, int numAnimFrames, int animFrameTime,
                             int numMotionFrames, int motionFrameTime, int motionRelative,
                             CStaticObject::MotionFrame* motion, LevelFileContentsStruct_* resource)
    : CStaticObject(mesh, numAnimFrames, animFrameTime, numMotionFrames, motionFrameTime,
                    motionRelative, motion) {
    m_bsgo.m_object = NULL;
    m_bsgo.m_pNext = NULL;
    m_bsgo.m_pPrev = NULL;
    m_pResource = resource;
    m_deleted = 0;
    m_marked = 0;
    m_spin.x = 0.0f;
    m_spin.y = 0.0f;
    m_spin.z = 0.0f;
    m_spin.w = 1.0f;
}

CThrownObject::~CThrownObject() {}

// Resets the physics and builds the bounding spheres from the mesh box.
int CThrownObject::Init() {
    CStaticObject::Init();
    m_lifeTime = PropI(this, PROP_LIFETIME);
    m_recyclable = PropI(this, PROP_RECYCLABLE) != 0;
    m_lastVelocity.z = 0.0f;
    m_lastVelocity.y = 0.0f;
    m_lastVelocity.x = 0.0f;
    m_move.z = 0.0f;
    m_move.y = 0.0f;
    m_move.x = 0.0f;
    m_gravity = 1;
    m_onGround = 0;
    m_pushed = 0;
    m_deleted = 0;
    m_playSounds = 0;
    m_marked = 0;
    m_state = -1;

    MSHNode* bounds = m_pMesh->m_pData->m_nodes;
    CVector3 axisX = bounds->m_axisX;
    CVector3 axisY = bounds->m_axisY;
    float radius = axisX.Length();
    CVector3 axisZ = bounds->m_axisZ;
    float lenY = axisY.Length();
    float lenZ = axisZ.Length();
    if (!(radius > lenY)) {
        radius = lenY;
    }
    if (!(radius > lenZ)) {
        radius = lenZ;
    }
    CVolSphere* local = AllocateSphereVolume();
    local->SetRadius(radius);
    local->SetCenter(m_pMesh->m_pData->m_nodes->m_center);
    SetLocalBoundingVolume((ISceneNode::EVolumeType)3, local);
    SetLocalBoundingVolume((ISceneNode::EVolumeType)0, local);
    CVolSphere* world = AllocateSphereVolume();
    *world = *local;
    SetWorldBoundingVolume((ISceneNode::EVolumeType)3, world);
    SetWorldBoundingVolume((ISceneNode::EVolumeType)0, world);
    m_bsgo.m_proximity[1] = 0;
    m_bsgo.m_proximity[0] = 0;
    m_bsgo.m_proximity[3] = 0;
    m_bsgo.m_proximity[2] = 0;
    m_bsgo.m_object = NULL;
    m_bsgo.m_node = this;
    m_impactSound = 0;
}

void CThrownObject::CleanUp() {
    const IVolume* local = GetLocalBoundingVolume((ISceneNode::EVolumeType)3);
    if (local != NULL) {
        FreeSphereVolume((void*)local);
    }
    const IVolume* world = GetWorldBoundingVolume((ISceneNode::EVolumeType)3);
    if (world != NULL && world != local) {
        FreeSphereVolume((void*)world);
    }
    m_pLocalVolume = NULL;
    m_pWorldVolume = NULL;
}

// Free objects collect the contact normals they move into (for
// ConstrainVelocity) and note landing; thrown ones report hitting a
// character (event 0xE7) and landing (event 0xE8, then they stop).
void CThrownObject::OnCollision(const CCollision& clsn) {
    bool first = clsn.m_pNode0 == this;
    ISceneNode* other = first ? clsn.m_pNode1 : clsn.m_pNode0;
    switch (m_state) {
    case -1:
        if (other->AsBullet() != NULL) {
            return;
        }
        for (SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
            CVector3 normal;
            if (first) {
                normal.x = -c->m_normal.x;
                normal.y = -c->m_normal.y;
                normal.z = -c->m_normal.z;
            } else {
                normal = c->m_normal;
            }
            if (!(m_velocity.z * normal.z + (m_velocity.x * normal.x + m_velocity.y * normal.y) >=
                  0.0f)) {
                m_pushDir.x = m_pushDir.x + normal.x;
                m_pushDir.y = m_pushDir.y + normal.y;
                m_pushDir.z = m_pushDir.z + normal.z;
                m_pushed = 1;
            }
            if (c->m_normal.z > 0.7f) {
                m_onGround = 1;
                PickImpactSound(this, other, c);
            }
        }
        return;
    case 0:
        return;
    case 1:
        if (other->AsAnimObject() != NULL || other->AsPlayerObject() != NULL) {
            BSObjectTriggerEvent(GetScriptObject(), 0xE7, other->GetScriptObject(), NULL, false);
            g_scene.ExcludeCollision(*this, *other, -1);
            return;
        }
        // fall through
    default:
        for (SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
            if (c->m_normal.z > 0.7f) {
                m_onGround = 1;
                if (m_state != 2) {
                    BSObjectTriggerEvent(GetScriptObject(), 0xE8, NULL, NULL, false);
                    m_state = 2;
                }
                CVector3 zero(0.0f, 0.0f, 0.0f);
                m_velocity.Copy(zero);
                PickImpactSound(this, other, c);
            }
        }
        break;
    }
}

// Bounces off the surfaces it moved into: the speed into them is reversed
// and scaled by the bounce, the speed along them by the friction.
void CThrownObject::ConstrainVelocity() {
    if (!m_pushed) {
        return;
    }
    m_pushDir.Normalize();
    CVector3 n = m_pushDir;
    float d = m_velocity.z * n.z + (m_velocity.x * n.x + m_velocity.y * n.y);
    if (!(d < 0.0f)) {
        m_pushed = 0;
        return;
    }
    float d2 = 2.0f * d;
    m_velocity.x = m_velocity.x - n.x * d2;
    m_velocity.y = m_velocity.y - n.y * d2;
    m_velocity.z = m_velocity.z - n.z * d2;

    CVector3 v = m_velocity;
    n = m_pushDir;
    int playSounds = m_playSounds;
    float along = n.z * v.z + (n.x * v.x + n.y * v.y);
    CVector3 normal;
    normal.x = along * n.x;
    normal.y = along * n.y;
    normal.z = along * n.z;
    CVector3 tangent;
    tangent.x = v.x - normal.x;
    tangent.y = v.y - normal.y;
    tangent.z = v.z - normal.z;
    if (playSounds == 0 || m_onGround) {
        float bounce = PropF(this, PROP_BOUNCE);
        normal.x *= bounce;
        normal.y *= bounce;
        normal.z *= bounce;
    }
    if (normal.z * normal.z + (normal.x * normal.x + normal.y * normal.y) < 0.0001f) {
        normal.z = 0.0f;
        normal.y = 0.0f;
        normal.x = 0.0f;
    }
    if (m_onGround) {
        float keep = 1.0f - PropF(this, PROP_FRICTION);
        tangent.x *= keep;
        tangent.y *= keep;
        tangent.z *= keep;
    } else if (playSounds == 0) {
        float keep = 1.0f - PropF(this, PROP_FRICTION);
        keep = keep * keep;
        keep = keep * keep;
        keep = keep * keep;
        tangent.x *= keep;
        tangent.y *= keep;
        tangent.z *= keep;
    }
    normal.x += tangent.x;
    normal.y += tangent.y;
    normal.z += tangent.z;
    m_velocity.Copy(normal);
    if (!m_onGround) {
        // Step off the surface.
        float step = 2.0f * CONTACT_EPSILON;
        CVector3 pos = *(const CVector3*)m_tm.m[3];
        CVector3 newPos;
        newPos.x = pos.x + step * m_pushDir.x;
        newPos.y = pos.y + step * m_pushDir.y;
        newPos.z = pos.z + step * m_pushDir.z;
        m_tm.SetPos(newPos);
    }
}

// Rolls the mesh about the axis across its motion when moving fast enough.
void CThrownObject::Draw(CDrawContext& ctx) {
    CMatrix tm;
    if (m_velocity.Length() > PropF(this, PROP_ROLL_SPEED)) {
        CVolSphere* sphere = (CVolSphere*)m_pLocalVolume;
        CVector3 axis;
        float zz = 0.0f * m_velocity.z;
        axis.x = -(1.0f * m_velocity.y - zz);
        axis.y = 1.0f * m_velocity.x - zz;
        axis.z = 0.0f * m_velocity.y - 0.0f * m_velocity.x;
        axis.Normalize();
        float radius;
        if (sphere != NULL) {
            radius = sphere->GetRadius();
        } else {
            radius = 1.0f;
        }
        tm.BuildRot(axis, m_move.Length() / radius);
        CQuaternion q;
        q.SetFromMatrix(tm);
        m_spin.Multiply(q);
    }
    m_spin.GetMatrix(tm);
    tm.Multiply(tm, m_tm);
    ctx.m_pWorld = &tm;
    if ((u8)m_pMesh->IsLightingEnabled()) {
        CStaticMesh::SetLightVolume(m_lightVolumes.GetVolume());
    }
    m_pMesh->Draw(ctx, m_animFrame);
}

// m_motionPos holds the position at the start of the frame.
int CThrownObject::BeginUpdate(float dt) {
    m_lightVolumes.Update(dt);
    m_motionPos = *(const CVector3*)m_tm.m[3];
    m_lastVelocity.Copy(m_velocity);
    ForceUpdateProximityTriggerStatus(&m_bsgo, true);
    if (m_impactSound > 0) {
        if (m_playSounds == 1) {
            PlayImpactSound(m_impactSound, 4, m_motionPos, false);
        }
        m_impactSound = -1;
    }
    if (m_lifeTime > 0.0f && m_state != 0) {
        m_lifeTime = m_lifeTime - (int)dt;
        if (m_lifeTime < 0.0f) {
            m_lifeTime = 0.0f;
        }
    }
}

int CThrownObject::AttemptUpdate(float dt) {
    if (m_deleted) {
        return;
    }
    m_pushDir.z = 0.0f;
    m_pushDir.y = 0.0f;
    m_pushDir.x = 0.0f;
    if (m_gravity && m_state != 0) {
        m_velocity.z = -GRAVITY * dt + m_lastVelocity.z;
    }
    ClampSpeed(this);
    m_move.x = dt * m_velocity.x;
    m_move.y = dt * m_velocity.y;
    m_move.z = dt * m_velocity.z;
    CVector3 pos;
    pos.x = m_motionPos.x + m_move.x;
    pos.y = m_motionPos.y + m_move.y;
    pos.z = m_motionPos.z + m_move.z;
    m_tm.SetPos(pos);
    UpdateWorldVolume();
}

// Removes the object once its lifetime runs out.
int CThrownObject::CommitUpdate() {
    if (m_deleted) {
        return;
    }
    if (m_lifeTime > 0.0f) {
        if (m_pWorldBound != m_pWorldVolume && m_pWorldBound != NULL && m_pLocalVolume != NULL) {
            m_pWorldBound->TransformedCopy(*m_pLocalVolume, m_tm);
        }
        ConstrainVelocity();
        m_motionPos = *(const CVector3*)m_tm.m[3];
        m_lastVelocity.Copy(m_velocity);
        if (m_onGround) {
            m_gravity = 0;
        } else {
            m_gravity = 1;
        }
        m_onGround = 0;
        m_pushed = 0;
        return;
    }
    MarkForDestruction(0);
}

// Free objects get knocked along by bullets.
void CThrownObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    if (bullet->AsProjectile() != NULL && m_state == -1) {
        CVector3 v;
        bullet->GetVelocity(v);
        float push = PropF(this, PROP_BULLET_PUSH);
        v.x = v.x * push;
        v.y *= push;
        v.z = v.z * push;
        m_velocity.x = m_velocity.x + v.x;
        m_velocity.y = m_velocity.y + v.y;
        m_velocity.z = m_velocity.z + v.z;
        ClampSpeed(this);
        m_lastVelocity.Copy(m_velocity);
        const SClsnContact* contact = clsn.m_pContacts;
        if (m_playSounds == 1) {
            PlayImpactSound(0xF, 1, contact->m_point, false);
        }
    }
}

void CThrownObject::SetVelocity(CVector3& v) {
    m_velocity.Copy(v);
    ClampSpeed(this);
    m_lastVelocity.Copy(m_velocity);
}

void CThrownObject::MarkForDestruction(int flags) {
    if (!m_marked) {
        CStaticObject::MarkForDestruction(flags);
        m_marked = 1;
    }
}

void CThrownObject::Destroy() {
    m_marked = 0;
    DeleteSpecialThrownObject(this);
}

void CThrownObject::SetDeleted(bool deleted) {
    m_deleted = deleted;
}

void CThrownObject::ScaleFromScript(float, float, EBSEventEnum) {}

void CThrownObject::RotateFromScript(CVector3&, CVector3&, EBSEventEnum) {}

void CThrownObject::TranslateFromScript(CVector3&, CVector3&, EBSEventEnum) {}

CThrownObject* CThrownObject::AsThrownObject() {
    return this;
}

int CThrownObject::GetObjectType() const {
    return 4;
}
