// CThrownBullet: grenades and other thrown rounds.
#include "game/thrownbullet.h"
#include "game/bulletfactory.h"
#include "game/explosion.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/IVolume.h"
#include "game/matrix.h"
#include "game/scene.h"
#include "game/player.h"
#include "game/Soldier_object.h"
#include "game/weapon.h"
#include "game/AIDoodad.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/static_obj.h"
#include "game/staticobjfactory.h"
#include "game/staticmesh.h"
#include "game/draw_context.h"
#include "game/light.h"
#include "game/particlesystem.h"
#include "game/publisher.h"
#include "game/proxtrig.h"
#include "game/objcreate.h"
#include "game/bstypes.h"
#include "game/bsobject.h"
#include "game/bsmessage.h"
#include "game/sound.h"
#include "game/ShellMenu.h"
#include "game/Moh2.h"
#include "game/MathFun.h"
#include "stdlib.h"

#define BIG_FLOAT 3.4028235e38f

float MP_THROWNBULLET_DRAW_DELAY = 2.0f;

extern bool g_bIsRubberGrenadeCheatEnabled;

typedef void (*SubjectCallback)(IObserver&, ISubject**, ESubjectEvent);

// Points a subscribed pointer at `subject` (the callback clears it).
static inline void SetSubscribed(IObserver& observer, ISubject** slot, ISubject* subject, SubjectCallback callback) {
    if (subject != *slot) {
        if (*slot != NULL) {
            CPublisher::CancelSubscription(observer, slot);
        }
        *slot = subject;
        if (subject != NULL) {
            CPublisher::AddSubscription(observer, slot, callback);
        }
    }
}

static inline void ClearSubject(IObserver& observer, ISubject** slot) {
    if (*slot != NULL) {
        CPublisher::CancelSubscription(observer, slot);
        *slot = NULL;
    }
}

// The orientation basis for a direction: like the projectile's, the up axis
// stays near world up (world x for steep directions).
static inline void BuildBasis(const CVector3& front, CVector3& right, CVector3& up) {
    if ((float)fabs(front.z) < (float)fabs(front.x)) {
        CVector3 z(0.0f, 0.0f, 1.0f);
        up = z;
        right.x = front.y * z.z - front.z * z.y;
        right.y = front.z * z.x - front.x * z.z;
        right.z = front.x * z.y - front.y * z.x;
        up.x = right.y * front.z - right.z * front.y;
        up.y = right.z * front.x - right.x * front.z;
        up.z = right.x * front.y - right.y * front.x;
    } else {
        if (front.y < 0.0f) {
            right.y = 0.0f;
            right.x = -1.0f;
            right.z = 0.0f;
        } else {
            right.y = 0.0f;
            right.x = 1.0f;
            right.z = 0.0f;
        }
        up.x = right.y * front.z - right.z * front.y;
        up.y = right.z * front.x - right.x * front.z;
        up.z = right.x * front.y - right.y * front.x;
        right.z = front.x * up.y - front.y * up.x;
        right.x = front.y * up.z - front.z * up.y;
        right.y = front.z * up.x - front.x * up.z;
    }
}

int CThrownBullet::Init() {
    return CBullet::Init();
}

const IVolume* CThrownBullet::GetLocalBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_localFiber;
}

const IVolume* CThrownBullet::GetWorldBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_worldFiber;
}

int CThrownBullet::IsCollisionEnabled() const {
    return 1;
}

int CThrownBullet::GetAttachedLight() const {
    return (int)PTR_TO_ADDR(m_pLight);
}

float CThrownBullet::GetDamage() const {
    return m_pProps->m_damage;
}

CThrownBullet* CThrownBullet::AsThrown() {
    return this;
}

void CThrownBullet::GetVelocity(CVector3& v) {
    v = m_velocity;
}

BPDLightVolume* CThrownBullet::GetLightVolume() {
    return m_lightVolumes.GetVolume();
}

void CThrownBullet::ExitLightVolume(BPDLightVolume* volume) {
    m_lightVolumes.RemoveVolume(volume);
}

void CThrownBullet::EnterLightVolume(BPDLightVolume* volume) {
    m_lightVolumes.AddVolume(volume);
}

void CThrownBullet::SomebodyCaughtYou() {
    if (!m_explode && !m_dead) {
        m_explode = false;
        m_dud = true;
        MarkForDestruction(0);
    }
}

// Script info for the grenade reaction events.
struct SGrenadeInfo {
    /* 0x0 */ PTR32(BSObject) m_thrower;
    /* 0x4 */ PTR32(CThrownBullet) m_grenade;
    /* 0x8 */ int m_numFriends;  // other soldiers nearby on the same side
};
FILE_SIZE_ASSERT(SGrenadeInfo, 0xC);  // event data; scripts read it by offset

static inline CAIObject* SoldierAI(CSoldierObject* soldier) {
    return soldier->GetAIDoodad()->m_pFilter->m_pAIObject;
}

// Teams 0..2 that differ are hostile.
static inline bool Hostile(u8 a, u8 b) {
    switch (a) {
    case 0:
        return b == 1 || b == 2;
    case 1:
        return b == 2;
    case 2:
        return b == 1;
    }
    return false;
}

void CThrownBullet::CheckEnemies() {
    float bestDist = 0.0f;
    BSGO_Basic* closest = NULL;
    int event = -1;
    if (m_noticed) {
        return;
    }
    if (m_type == 0 || (u32)(m_type - 1) <= 1 || (u32)(m_type - 5) <= 5 || m_type == 8) {
        return;
    }
    for (BSGO_Basic* g = g_pGermanList; g != NULL; g = g->m_pNext) {
        CSoldierObject* soldier = g->GetSceneNode()->AsSoldierObject();
        if (m_pFiredBy != NULL && soldier == m_pFiredBy->AsSoldierObject()) {
            continue;
        }
        if (SoldierAI(soldier)->m_status.GetVarValue((aistatus_var)0) <= 0) {
            continue;
        }
        CVector3 d;
        soldier->GetPosition(d);
        d.x = m_basePos.x - d.x;
        d.y = m_basePos.y - d.y;
        d.z = m_basePos.z - d.z;
        float speed = m_velocity.Length();
        if (d.z > 1.3f && d.z < 1.9f) {
            // Head height: in front of the soldier, near his weapon hand.
            CVector3 axis;
            soldier->GetForward(axis);
            float along = axis.x * d.x + axis.y * d.y + axis.z * d.z;
            if (along >= 1.0f || along <= 0.0f) {
                continue;
            }
            float lo = -0.1f;
            float hi = 0.5f;
            CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
            if (weapon != NULL && weapon->m_pProperties->m_hand != 0) {
                lo = -0.3f;
                hi = 0.2f;
            }
            soldier->GetRightward(axis);
            float side = axis.x * d.x + axis.y * d.y + axis.z * d.z;
            if (side < hi && side > lo) {
                m_noticed = true;
                closest = g;
                event = 0xA3;
                break;
            }
        } else if (d.z < 0.05f && speed < 0.05f) {
            // Lying still on the ground nearby.
            float distSq = d.x * d.x + d.y * d.y;
            if (distSq < 9.0f) {
                if (closest == NULL || distSq < bestDist) {
                    m_noticed = true;
                    bestDist = distSq;
                    closest = g;
                    event = 0xAC;
                }
            } else if (distSq < 36.0f) {
                if (closest == NULL || distSq < bestDist) {
                    m_noticed = true;
                    bestDist = distSq;
                    closest = g;
                    event = 0xAD;
                }
            }
        }
    }
    if (closest == NULL) {
        return;
    }
    SGrenadeInfo info;
    info.m_thrower = m_pFiredBy != NULL ? m_pFiredBy->GetScriptObject() : NULL;
    info.m_grenade = this;
    info.m_numFriends = 0;
    CAIObject* closestAI = closest->GetSceneNode()->GetAIDoodad()->m_pFilter->m_pAIObject;
    for (BSGO_Basic* g = g_pGermanList; g != NULL; g = g->m_pNext) {
        CSoldierObject* soldier = g->GetSceneNode()->AsSoldierObject();
        if (m_pFiredBy != NULL && soldier == m_pFiredBy->AsSoldierObject()) {
            continue;
        }
        if (g == closest) {
            continue;
        }
        CAIFilterObject* filter = soldier->GetAIDoodad()->m_pFilter;
        if (filter->m_pAIObject->m_status.GetVarValue((aistatus_var)0) <= 0) {
            continue;
        }
        CVector3 d;
        soldier->GetPosition(d);
        d.x = m_basePos.x - d.x;
        d.y = m_basePos.y - d.y;
        d.z = m_basePos.z - d.z;
        if (d.x * d.x + d.y * d.y >= 36.0f) {
            continue;
        }
        BSObjectTriggerEvent(g->m_object, 0xAE, &info, NULL, true);
        if (!Hostile(closestAI->field_0x14, filter->m_pAIObject->field_0x14)) {
            info.m_numFriends++;
        }
    }
    BSObjectTriggerEvent(closest->m_object, (u16)event, &info, NULL, true);
}

// Thrown along `dir`: strength is force / 512 times the type's weight.
void CThrownBullet::ApplyForceTo(CVector3 dir, float force) {
    float scale = force * 0.001953125f;
    m_velocity = dir;
    m_velocity.Normalize();
    float speed = m_pProps->m_weight * scale;
    m_velocity.x *= speed;
    m_velocity.y *= speed;
    m_velocity.z *= speed;
    m_prevVelocity = m_velocity;
}

void CThrownBullet::Halt() {
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_prevVelocity.z = 0.0f;
    m_prevVelocity.y = 0.0f;
    m_prevVelocity.x = 0.0f;
}

// Shot: sets it off.
void CThrownBullet::HandleBulletCollision(CBullet* bullet, const CCollision&) {
    if (bullet->AsProjectile() != NULL) {
        m_explode = !m_dud;
    }
}

void CThrownBullet::SetDetonationTime(float time) {
    if (m_pProps->m_timed) {
        field_0x90 = time;
    }
}

void CThrownBullet::Explode() {
    m_explode = !m_dud;
}

int CThrownBullet::GetScriptBulletType() const {
    switch (m_type) {
    case 3:
        return 3;
    case 0:
    case 8:
    case 10:
        return 0x15;
    case 1:
        return 0x17;
    case 2:
        return 0x16;
    case 4:
        return 0x18;
    case 5:
        return 0x18;
    case 9:
        return 0x18;
    case 6:
        return 0x1A;
    default:
        DebugMsg("WARNING: Unknown thrown bullet type in GetScriptBulletType.  Type %d\n", m_type);
        return 3;
    }
}

int CThrownBullet::IsVisible(CDrawContext& ctx) const {
    return m_worldFiber.TestVisibility(ctx);
}

// The type's model, tumbling.
void CThrownBullet::Draw(CDrawContext& ctx) {
    if (g_bInMultiplayerMode && m_pFiredBy != NULL && m_pFiredBy->AsPlayerObject() != NULL &&
        field_0x90 > (float)m_pProps->m_lifetime - MP_THROWNBULLET_DRAW_DELAY) {
        return;
    }
    CStaticObject* model = NULL;
    switch (m_type) {
    case 3:
        model = g_pStaticObjectGrenade;
        break;
    case 4:
        model = g_pStaticObjectStickGrenade;
        break;
    case 5:
        model = g_pStaticObjectKnife;
        break;
    case 9:
        model = g_pStaticObjectWrench;
        break;
    case 0:
    case 1:
    case 2:
    case 8:
    case 10:
        model = g_pStaticObjectBazooka;
        break;
    default:
        DebugMsg("WARNING: Unknown thrown bullet type in Draw.  Type %d\n", m_type);
        break;
    }
    if (model == NULL) {
        return;
    }
    CMatrix rot;
    CMatrix world;
    rot.BuildRotX(m_angles.x);
    rot.RotateY(m_angles.y);
    rot.RotateZ(m_angles.z);
    world.Multiply(rot, *(CMatrix*)&m_rightward);
    CStaticMesh* mesh = model->m_pMesh;
    if (mesh == NULL) {
        return;
    }
    ctx.m_pWorld = &world;
    if (mesh->IsLightingEnabled()) {
        CStaticMesh::SetLightVolume(GetLightVolume());
    }
    mesh->Draw(ctx, 0);
}

int CThrownBullet::GetCollisionId() const {
    return m_collisionId;
}

// The blast: explosion, effects, sound and script message 0xB4.
static inline void Detonate(CThrownBullet* bullet, const CVector3& pos, bool ground, CPlayerObject* listener,
                            s64 soundLo, s64 soundHi) {
    CExplosion explosion(g_scene, pos, bullet->m_pProps->m_radius, 1.0f, bullet->GetDamage(),
                         (EScriptBulletType)bullet->GetScriptBulletType(), bullet->GetFiredBy(), false);
    CMatrix tm;
    tm.BuildTrans(pos);
    CreateExplosionEffect(tm, ground, g_cheats.field_0xB);
    CVector3 d;
    listener->GetPosition(d);
    d.x -= pos.x;
    d.y -= pos.y;
    d.z -= pos.z;
    int sound[10];
    sound[0] = 7;
    sound[1] = 3;
    sound[2] = (int)MathFunRandomI64(soundLo, soundHi);
    sound[3] = (int)MathFunRandomI64(3600, 4600);
    sound[4] = (u16)GetAzimuth(pos, listener);
    sound[5] = (int)(256.0f * d.Length());
    sound[6] = 0;
    sound[7] = 0x61A7;
    sound[8] = 0;
    sound[9] = 0x7D00;
    AEMS_SendEvent(sound);
    BSSendMessage(0xB4, bullet->GetScriptObject(), NULL, NULL, bullet);
}

// Bouncers keep going until the fuse runs out (or something sets them
// off); the others go off on contact, pulled back 5cm from the surface.
int CThrownBullet::CommitUpdate() {
    if (m_dead) {
        RETURN_NOTHING;
    }
    switch (m_type) {
    case 3:
    case 4:
    case 5:
    case 9:
        if (field_0x90 > 0.0f && (!m_explode || m_dud)) {
            ConstrainVelocity();
            m_basePos = m_position;
            m_prevVelocity = m_velocity;
            if (m_type != 5) {
                m_gravity = !m_onGround;
            }
            m_onGround = false;
            m_collided = false;
        } else {
            if (!m_dud) {
                CVector3 pos(m_position);
                CPlayerObject* listener;
                if (g_bInMultiplayerMode) {
                    listener = g_scene.FindClosestPlayer(pos);
                } else {
                    listener = g_scene.GetPlayer(0);
                }
                Detonate(this, pos, m_onGround, listener, 0x10, 0x11);
            }
            MarkForDestruction(0);
        }
        break;
    case 0:
    case 1:
    case 2:
    case 8:
    case 10:
        if (field_0x90 > 0.0f && (!m_explode || m_dud)) {
            ConstrainVelocity();
            m_basePos = m_position;
            m_prevVelocity = m_velocity;
        } else {
            CVector3 pos(m_position);
            float speed = m_velocity.Length();
            if (speed > CONTACT_EPSILON) {
                float back = -0.05f / speed;
                pos.x += back * m_velocity.x;
                pos.y += back * m_velocity.y;
                pos.z += back * m_velocity.z;
            }
            Detonate(this, pos, field_0x90 > 0.0f, g_scene.GetPlayer(0), 0x22, 0x23);
            MarkForDestruction(0);
        }
        break;
    default:
        DebugMsg("WARNING: Unknown thrown bullet type in CommitUpdate.  Type %d\n", m_type);
        break;
    }
    if (m_pLight != NULL) {
        CMatrix tm;
        GetTMLocalToWorld(tm);
        ((IMovingSceneNode*)m_pLight)->SetTMLocalToWorld(tm);
    }
    END_RETURN_NOTHING
}

void CreateExplosionEffect(const CMatrix& tm, bool ground, bool cheat) {
    CParticleSystem* ps;
    if (!cheat) {
        ps = CParticleSystem::Create(0xFB394D09);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
        ps = CParticleSystem::Create(0x4F2B8179);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
        ps = CParticleSystem::Create(0x1CD6E4B5);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
        if (ground) {
            ps = CParticleSystem::Create(0x4822CD8F);
            if (ps != NULL) {
                ps->SetLocalToWorld(tm);
            }
        }
    } else {
        ps = CParticleSystem::Create(0xABD02935);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
        ps = CParticleSystem::Create(0x6BD7A8DB);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
        ps = CParticleSystem::Create(0x28E4D6EF);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
        ps = CParticleSystem::Create(0xFF8E1F13);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
    }
    CLight* light = CLight::Create(0x9BCE1511);
    if (light != NULL) {
        CVector3 pos(*(const CVector3*)tm.m[3]);
        pos.z += 0.5f;
        light->SetPosition(pos);
    }
}

int CThrownBullet::AttemptUpdate(float dt) {
    if (m_dead) {
        RETURN_NOTHING;
    }
    field_0x94 = BIG_FLOAT;
    m_velocity = m_prevVelocity;
    if (m_gravity) {
        m_velocity.z += -0.005455351f * dt;
    }
    CVector3 move(dt * m_velocity.x, dt * m_velocity.y, dt * m_velocity.z);
    float dist = move.Length();
    m_angles.x = dist * m_spin.x;
    m_angles.y = dist * m_spin.y;
    m_angles.z = dist * m_spin.z;
    m_angles.x += m_prevAngles.x;
    m_angles.y += m_prevAngles.y;
    m_angles.z += m_prevAngles.z;
    move.x += m_basePos.x;
    move.y += m_basePos.y;
    move.z += m_basePos.z;
    ((CMatrix*)&m_rightward)->SetPos(move);
    m_worldFiber.Set(m_basePos, move);
    CVector3 min;
    CVector3 max;
    m_worldFiber.GetExtents(min, max);
    m_pMinX->m_value = min.x - CONTACT_EPSILON;
    m_pMinY->m_value = min.y - CONTACT_EPSILON;
    m_pMaxX->m_value = CONTACT_EPSILON + max.x;
    m_pMaxY->m_value = CONTACT_EPSILON + max.y;
    END_RETURN_NOTHING
}

// Bouncers turn to face where they're going (keeping their right axis).
int CThrownBullet::BeginUpdate(float dt) {
    m_lightVolumes.Update(dt);
    CheckEnemies();
    m_basePos = m_position;
    switch (m_type) {
    case 0:
    case 1:
    case 2:
    case 8:
    case 10:
        break;
    case 3:
    case 4:
    case 5:
    case 9:
        if (!m_onGround && m_velocity.Length() > CONTACT_EPSILON) {
            CVector3 front(m_velocity);
            front.Normalize();
            CVector3 right;
            GetRightward(right);
            CVector3 up;
            up.x = right.y * front.z - right.z * front.y;
            up.y = right.z * front.x - right.x * front.z;
            up.z = right.x * front.y - right.y * front.x;
            right.x = front.y * up.z - front.z * up.y;
            right.y = front.z * up.x - front.x * up.z;
            right.z = front.x * up.y - front.y * up.x;
            right.Normalize();
            up.Normalize();
            CMatrix& tm = *(CMatrix*)&m_rightward;
            tm.SetFront(front);
            tm.SetRight(right);
            tm.SetUp(up);
        }
        break;
    default:
        DebugMsg("WARNING: Unknown thrown bullet type in BeginUpdate.  Type %d\n", m_type);
        break;
    }
    m_prevVelocity = m_velocity;
    m_prevAngles = m_angles;
    ForceUpdateProximityTriggerStatus(&m_bsgo, true);
    if (field_0x90 > 0.0f) {
        field_0x90 -= dt;
        if (field_0x90 < 0.0f) {
            field_0x90 = 0.0f;
        }
    }
    field_0x94 = BIG_FLOAT;
    END_RETURN_NOTHING
}

// The bounce: reflect off the summed contact normal, keep m_bounce of the
// normal part and lose m_friction of the sliding part on flat ground
// (rubber-grenade cheat: keep the speed), then step off the surface.
void CThrownBullet::ConstrainVelocity() {
    if (!m_collided) {
        return;
    }
    float speedBefore = 1.0f;
    if (g_cheats.field_0xA) {
        speedBefore = m_velocity.Length();
    }
    m_normal.Normalize();
    float into = m_velocity.x * m_normal.x + m_velocity.y * m_normal.y + m_velocity.z * m_normal.z;
    if (into > 0.0f) {
        m_normal.x = -m_normal.x;
        m_normal.y = -m_normal.y;
        m_normal.z = -m_normal.z;
        into = m_velocity.x * m_normal.x + m_velocity.y * m_normal.y + m_velocity.z * m_normal.z;
    }
    float twice = 2.0f * into;
    m_velocity.x -= m_normal.x * twice;
    m_velocity.y -= m_normal.y * twice;
    m_velocity.z -= m_normal.z * twice;
    float along = m_normal.x * m_velocity.x + m_normal.y * m_velocity.y + m_normal.z * m_velocity.z;
    CVector3 normalPart(along * m_normal.x, along * m_normal.y, along * m_normal.z);
    CVector3 slide(m_velocity.x - normalPart.x, m_velocity.y - normalPart.y, m_velocity.z - normalPart.z);
    normalPart.x *= m_pProps->m_bounce;
    normalPart.y *= m_pProps->m_bounce;
    normalPart.z *= m_pProps->m_bounce;
    if (normalPart.x * normalPart.x + normalPart.y * normalPart.y + normalPart.z * normalPart.z < 0.0001f &&
        m_onGround) {
        normalPart.z = 0.0f;
        normalPart.y = 0.0f;
        normalPart.x = 0.0f;
    }
    float flat = 0.0f;
    float up = 0.0f * m_normal.x + 0.0f * m_normal.y + 1.0f * m_normal.z;
    if (!(up <= 0.0f)) {
        flat = up * (up * up);
    }
    float keep = 1.0f - flat * m_pProps->m_friction;
    normalPart.x += slide.x * keep;
    normalPart.y += slide.y * keep;
    normalPart.z += slide.z * keep;
    m_velocity = normalPart;
    if (g_cheats.field_0xA) {
        float speed = m_velocity.Length();
        if (speed > 0.01 && speedBefore > 0.01) {
            float scale = speedBefore / speed;
            m_velocity.x *= scale;
            m_velocity.y *= scale;
            m_velocity.z *= scale;
        }
    }
    CVector3 step;
    if (m_onGround) {
        step.x = CONTACT_EPSILON * m_normal.x;
        step.y = CONTACT_EPSILON * m_normal.y;
        step.z = CONTACT_EPSILON * m_normal.z;
    } else {
        float eps = 2.0f * CONTACT_EPSILON;
        step.x = eps * m_normal.x;
        step.y = eps * m_normal.y;
        step.z = eps * m_normal.z;
    }
    CVector3 pos(m_position);
    ((CMatrix*)&m_rightward)->SetPos(CVector3(pos.x + step.x, pos.y + step.y, pos.z + step.z));
}

int CThrownBullet::Penetrate(const CCollision& clsn) {
    SClsnContact* contact = clsn.m_pContacts;
    CVector3 step;
    GetForward(step);
    float along = step.x * contact->m_normal.x + step.y * contact->m_normal.y + step.z * contact->m_normal.z;
    float absAlong = (float)fabs(along);
    if (absAlong > 0.00001f) {
        float scale = CONTACT_EPSILON / absAlong;
        step.x *= scale;
        step.y *= scale;
        step.z *= scale;
    } else if (along > 0.0f) {
        step.x = CONTACT_EPSILON * contact->m_normal.x;
        step.y = CONTACT_EPSILON * contact->m_normal.y;
        step.z = CONTACT_EPSILON * contact->m_normal.z;
    } else {
        float eps = -CONTACT_EPSILON;
        step.x = eps * contact->m_normal.x;
        step.y = eps * contact->m_normal.y;
        step.z = eps * contact->m_normal.z;
    }
    CVector3 pos(m_position);
    CVector3 next(pos.x + step.x, pos.y + step.y, pos.z + step.z);
    ((CMatrix*)&m_rightward)->SetPos(next);
    m_collided = false;
    m_explode = false;
    END_RETURN_NOTHING
}

void CThrownBullet::OnCollision(const CCollision& clsn) {
    if (m_dead) {
        return;
    }
    if (m_explode) {
        return;
    }
    SClsnContact* c;
    switch (m_type) {
    case 5:
        m_gravity = true;
        // fall through
    case 3:
    case 4:
    case 9:
        m_normal.z = 0.0f;
        m_normal.y = 0.0f;
        m_normal.x = 0.0f;
        for (c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
            if (c->m_normal.z > 0.7f &&
                c->m_normal.x * m_velocity.x + c->m_normal.y * m_velocity.y + c->m_normal.z * m_velocity.z <= 0.0f) {
                m_onGround = true;
            }
            m_normal.x += c->m_normal.x;
            m_normal.y += c->m_normal.y;
            m_normal.z += c->m_normal.z;
            m_collided = true;
        }
        break;
    case 0:
    case 1:
    case 2:
    case 8:
    case 10:
        for (c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
            if (c->m_normal.z > 0.7f) {
                m_onGround = true;
            }
            m_collided = true;
        }
        if (m_collided) {
            m_explode = !m_dud;
        }
        break;
    default:
        DebugMsg("WARNING: Unknown thrown bullet type in OnCollision.  Type %d\n", m_type);
        break;
    }
    if (m_collided) {
        ISceneNode* other = clsn.m_pNode0;
        if (other == this) {
            other = clsn.m_pNode1;
        }
        other->HandleBulletCollision(this, clsn);
    }
}

void CThrownBullet::Destroy() {
    g_pBulletFactory->DestroyBullet(this);
}

void CThrownBullet::MarkForDestruction(int when) {
    ISubject::MarkForDestruction(when);
    if (m_pLight != NULL) {
        ((ISubject*)m_pLight)->MarkForDestruction(when);
        ClearSubject(*this, (ISubject**)&m_pLight);
    }
    if (m_pTrail != NULL) {
        m_pTrail->DeActivate();
        m_pTrail = NULL;
    }
    m_dead = true;
    g_scene.Remove(*this);
    m_collisionId = -1;
}

void CThrownBullet::Shutdown() {
    CBullet::Shutdown();
    m_lightVolumes.Reset();
}

// Grenades get a light and a smoke trail; spinners tumble.
static inline void StartTrail(CThrownBullet* bullet, SubjectCallback callback) {
    bullet->m_gravity = false;
    bullet->m_angles.x = 0.0f;
    bullet->m_angles.y = 0.0f;
    bullet->m_angles.z = 0.0f;
    bullet->m_spin.x = 0.0f;
    bullet->m_spin.y = 0.0f;
    bullet->m_spin.z = 0.0f;
    CLight* light = CLight::Create(0xA2401755);
    if (light != NULL) {
        light->SetTMLocalToWorld(*(CMatrix*)&bullet->m_rightward);
        SetSubscribed(*bullet, (ISubject**)&bullet->m_pLight, (ISubject*)light, callback);
    }
    bullet->m_pTrail = CParticleSystem::Create(0xA23FD6BD);
    if (bullet->m_pTrail != NULL) {
        CVector3 v(60.0f * bullet->m_velocity.x, 60.0f * bullet->m_velocity.y, 60.0f * bullet->m_velocity.z);
        bullet->m_pTrail->SetSystemInitialVelocity(v);
        bullet->m_pTrail->SetLocalToWorld(*(CMatrix*)&bullet->m_rightward);
    }
}

static inline float RandomUnit() {
    return (float)rand() / 32767.0f;
}

static void SECB_NullifyPointer(IObserver& observer, ISubject** slot, ESubjectEvent) {
    if (*slot != NULL) {
        if (slot != NULL) {
            CPublisher::CancelSubscription(observer, slot);
        }
        *slot = NULL;
    }
}

// Thrown from `pos` along `dir` at `speed`; `power` (0..512) is how long
// the throw was held, mapped between 10% and full strength (soldiers throw
// at a fixed strength per type).
void CThrownBullet::Init(EThrownBulletTypes type, ThrownBulletProperties_struct* props, const CVector3& pos,
                         const CVector3& dir, float speed, EClsnId clsnId, float power, ISceneNode* firedBy) {
    CBullet::Init();
    m_type = type;
    m_pProps = props;
    field_0x90 = (float)props->m_lifetime;
    m_basePos = pos;
    m_collisionId = clsnId;
    m_onGround = false;
    m_collided = false;
    m_explode = false;
    f1F8_2 = false;
    m_noticed = false;
    m_dead = false;
    m_dud = !props->m_timed;
    ClearSubject(*this, (ISubject**)&m_pLight);
    m_pTrail = NULL;
    field_0x1FC = 0;
    SetSubscribed(*this, (ISubject**)&m_pFiredBy, (ISubject*)firedBy, SECB_NullifyPointer);

    float strength = power * 0.001953125f;
    static float maxPower = 1.0f;
    static float minPower = 0.1f;
    if (m_pFiredBy != NULL && m_pFiredBy->AsSoldierObject() != NULL) {
        switch (type) {
        case 0:
        case 1:
        case 2:
            strength = 0.25f * maxPower;
            break;
        case 10:
            strength = 0.5f * maxPower;
            break;
        default:
            strength = maxPower * strength;
            break;
        }
    } else {
        strength = strength * (maxPower - minPower) + minPower;
    }
    m_velocity = dir;
    m_velocity.Normalize();
    float scale = strength * (speed * m_pProps->m_weight);
    m_velocity.x *= scale;
    m_velocity.y *= scale;
    m_velocity.z *= scale;
    m_prevVelocity = m_velocity;

    CMatrix& tm = *(CMatrix*)&m_rightward;
    tm.BuildTrans(pos);
    CVector3 front(dir);
    front.Normalize();
    CVector3 right;
    CVector3 up;
    BuildBasis(front, right, up);
    tm.SetFront(front);
    tm.SetRight(right);
    tm.SetUp(up);
    g_scene.Add(*this);
    float length = m_velocity.Length();
    m_localFiber.Set(CVector3(0.0f, -length, 0.0f), CVector3(0.0f, 0.0f, 0.0f));

    switch (m_type) {
    case 5: {
        // Knife: tumbles end over end at random rates.
        m_gravity = false;
        m_angles.x = 0.0f;
        m_angles.y = 0.0f;
        m_angles.z = 0.0f;
        float a = RandomUnit();
        float b = RandomUnit();
        float c = RandomUnit();
        m_spin.x = -1.3962634f * c;
        m_spin.y = -1.3962634f * b;
        m_spin.z = -1.3962634f * a;
        break;
    }
    case 4:
        // Stick grenade.
        m_angles.x = 0.0f;
        m_angles.y = 0.0f;
        m_angles.z = 0.0f;
        m_spin.x = 0.0f;
        m_spin.y = 1.3962634f;
        m_spin.z = 1.3962634f;
        break;
    case 3:
    case 6:
    case 9: {
        m_angles.x = 0.0f;
        m_angles.y = 0.0f;
        m_angles.z = 0.0f;
        float a = RandomUnit();
        float b = RandomUnit();
        float c = RandomUnit();
        m_spin.x = -0.41887903f * c;
        m_spin.y = -0.41887903f * b;
        m_spin.z = -0.41887903f * a;
        break;
    }
    case 0:
    case 1:
    case 2:
    case 8:
    case 10:
        StartTrail(this, SECB_NullifyPointerOnMarked);
        break;
    default:
        DebugMsg("WARNING: Unknown thrown bullet type in Init.  Type %d\n", m_type);
        break;
    }
    m_prevAngles = m_angles;
    m_bsgo.m_proximity[1] = 0;
    m_bsgo.m_proximity[0] = 0;
    m_bsgo.m_proximity[3] = 0;
    m_bsgo.m_proximity[2] = 0;
    m_bsgo.m_object = NULL;
    m_bsgo.m_node = this;

    // Tell the thrower's script.
    CSoldierObject* thrower = NULL;
    if (m_pFiredBy != NULL) {
        thrower = m_pFiredBy->AsSoldierObject();
    }
    if (thrower != NULL) {
        for (BSGO_Basic* g = g_pGermanList; g != NULL; g = g->m_pNext) {
            if (g->GetSceneNode()->AsSoldierObject() == thrower) {
                BSObjectTriggerEvent(g->m_object, 0x108, this, NULL, true);
                break;
            }
        }
    }

    // Rubber grenades: no friction, full bounce (changes the shared type).
    if (g_bIsRubberGrenadeCheatEnabled || g_cheats.field_0xA) {
        m_pProps->m_friction = 0.0f;
        m_pProps->m_bounce = 1.0f;
    }
}

CThrownBullet::~CThrownBullet() {}

CThrownBullet::CThrownBullet() {
    m_bsgo.m_object = NULL;
    m_bsgo.m_pNext = NULL;
    m_bsgo.m_pPrev = NULL;
    field_0x28 = 2;
}

void* CThrownBullet::operator new[](unsigned long size) {
    return DWI_alloc(NULL, size, 0x400);
}
