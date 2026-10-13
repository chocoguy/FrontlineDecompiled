// CWeapon: held guns and throwables.
#include "game/weapon.h"
#include "game/bulletfactory.h"
#include "game/bullet.h"
#include "game/thrownbullet.h"
#include "game/weaponfactory.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/IVolume.h"
#include "game/line.h"
#include "game/fiber.h"
#include "game/matrix.h"
#include "game/scene.h"
#include "game/world_object.h"
#include "game/player.h"
#include "game/hierobject.h"
#include "game/proxtrig.h"
#include "game/bsbifunc.h"
#include "game/sound.h"
#include "game/soundtable.h"
#include "game/MathFun.h"
#include "game/Moh2.h"

// The basis axis perpendicular to `v` (rotation axis for the spread).
static inline void Perpendicular(const CVector3& v, CVector3& out) {
    if ((float)fabs(v.z) < (float)fabs(v.x)) {
        CVector3 z(0.0f, 0.0f, 1.0f);
        out.x = v.y * z.z - v.z * z.y;
        out.y = v.z * z.x - v.x * z.z;
        out.z = v.x * z.y - v.y * z.x;
    } else {
        CVector3 x(1.0f, 0.0f, 0.0f);
        out.x = x.y * v.z - x.z * v.y;
        out.y = x.z * v.x - x.x * v.z;
        out.z = x.x * v.y - x.y * v.x;
    }
}

static inline void Cross(CVector3& out, const CVector3& a, const CVector3& b) {
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
}

CWeapon* CWeapon::AsWeaponObject() {
    return this;
}

void CWeapon::GetPosition(CVector3& v) const {
    v = m_aimOrigin;
}

int CWeapon::Init() {
    return CStaticObject::Init();
}

// The flash shows for m_flashDuration, then turns off.
int CWeapon::GetMuzzleFlashStatus() {
    if (m_flashTime > m_flashDuration) {
        m_flashShown = false;
        m_muzzleFlash = false;
        m_muzzleFlash = false;
        m_flashTime = 0.0f;
    }
    return m_muzzleFlash;
}

void CWeapon::SetInfiniteAmo(bool infinite) {
    m_infiniteAmmo = infinite;
}

bool CWeapon::CanCook() const {
    return g_pBulletFactory->CanBulletTypeCook(m_pProperties->m_isProjectile != 0, m_pProperties->m_bulletType);
}

int CWeapon::GetWeaponSoundType() const {
    return m_pProperties->m_soundType;
}

float CWeapon::GetBulletSpeed() const {
    return m_pProperties->m_speed *
           g_pBulletFactory->GetBulletWeight(m_pProperties->m_isProjectile != 0, m_pProperties->m_bulletType);
}

// Clamped distance for the shot sound (8.8 fixed point).
static inline int SoundDistance(const CVector3& d) {
    int dist = (int)d.Length() << 8;
    if (dist > 0x7FFD) {
        dist = 0x7FFD;
    }
    return dist;
}

// One shot: m_numPellets bullets along m_aimDir from m_aimOrigin, each
// turned by a random spread. The player's grenades and rockets go 20
// degrees up instead; the AI lobs them at 30 degrees. Then the shot sound
// (tank guns use the scripted tank sound) and AI noise.
void CWeapon::FireBullet() {
    CBullet* bullet = NULL;
    CPlayerObject* player = NULL;
    for (int i = 0; i < m_pProperties->m_numPellets; i++) {
        CVector3 dir(m_aimDir);
        CVector3 origin(m_aimOrigin);
        CVector3 axis;
        Perpendicular(dir, axis);
        CMatrix m;
        m.BuildRot(axis, MathFunRandomReal(m_pProperties->m_spread, -m_pProperties->m_spread));
        m.Rotate(m_aimDir, MathFunRandomReal(0.0f, 3.1415927f));
        player = m_pHolder->AsPlayerObject();
        if (player != NULL) {
            m_pHolder->TriggerScriptEvent(0x18, NULL, false);
            if (m_pProperties->m_isProjectile == 0 &&
                (m_pProperties->m_bulletType == 4 || m_pProperties->m_bulletType == 3)) {
                CVector3 z(0.0f, 0.0f, 1.0f);
                CVector3 side;
                CVector3 up;
                CVector3 pitch;
                Cross(side, dir, z);
                Cross(up, side, dir);
                Cross(pitch, dir, up);
                m.BuildRot(pitch, 0.3490656f);
            }
        }
        m.TransformVector(dir, m_aimDir);
        if (m_pHolder != player && m_pProperties->m_isProjectile == 0 &&
            (m_pProperties->m_bulletType == 4 || m_pProperties->m_bulletType == 3)) {
            dir.z = 0.0f;
            dir.z = 0.577f * dir.Length();
        }
        bullet = g_pBulletFactory->CreateBullet(m_pProperties->m_bulletType, m_pProperties->m_isProjectile != 0,
                                                origin, dir, m_pProperties->m_speed * m_speedScale,
                                                (EClsnId)m_clsnId, m_power, m_pHolder);
        if (bullet == NULL) {
            m_refireTime += (float)m_pProperties->m_refireRate;
            return;
        }
        if (m_pProperties->m_isProjectile == 0) {
            CThrownBullet* thrown = bullet->AsThrown();
            if (m_fuse >= 0.0) {
                thrown->SetDetonationTime(m_fuse);
            }
        } else {
            m_muzzleFlash = true;
        }
    }
    if (!g_cheats.field_0x1) {
        m_clipAmmo--;
    }
    m_fired = true;

    int sound[8];
    sound[3] = 0;
    sound[4] = 0;
    sound[5] = 0;
    sound[6] = 0;
    int soundType = m_pProperties->m_soundType;
    bool tankGun = m_weaponType == 0x2C || (u32)(m_weaponType - 0x1F) <= 1 || m_weaponType == 0x1E;
    if (m_pHolder != NULL && m_pHolder->AsHierObject() != NULL &&
        ((CStaticObject*)m_pHolder->AsHierObject())->AsHierTankObject() != NULL && tankGun) {
        CSoundTable::SSoundParms parms;
        if (g_soundMapSingleton.GetSoundEvent(0x207, -1, 0x3B, &parms)) {
            CVector3 pos;
            m_pHolder->GetPosition(pos);
            PlayScriptedSound(m_pHolder, pos, parms, -1, NULL);
        }
    } else {
        if (m_pHolder->AsPlayerObject() != NULL) {
            sound[3] = 0;
            sound[4] = 0;
            sound[5] = 0;
            sound[6] = 0;
        } else if (m_pProperties->m_isProjectile == 0 || tankGun) {
            // Heard from the shooter and from the round.
            CPlayerObject* listener = g_scene.GetPlayer(0);
            CVector3 pos;
            m_pHolder->GetPosition(pos);
            sound[3] = (u16)GetAzimuth(pos, listener);
            CVector3 ear;
            listener->GetPosition(ear);
            pos.x -= ear.x;
            pos.y -= ear.y;
            pos.z -= ear.z;
            sound[5] = SoundDistance(pos);
            bullet->GetPosition(pos);
            sound[4] = (u16)GetAzimuth(pos, listener);
            pos.x -= ear.x;
            pos.y -= ear.y;
            pos.z -= ear.z;
            sound[6] = SoundDistance(pos);
        }
        sound[0] = 1;
        sound[1] = soundType;
        sound[2] = 1;
        sound[7] = m_clipAmmo;
        int stop = 9;
        AEMS_SendEvent(&stop);
        AEMS_SendEvent(sound);
    }
    // The player's grenade without a cooked fuse: the pin sound.
    if (m_pHolder->AsPlayerObject() != NULL && m_pProperties->m_bulletType == 3 &&
        m_pProperties->m_isProjectile == 0 && m_fuse < 0.0f) {
        sound[0] = 1;
        sound[1] = 0x18;
        sound[2] = 1;
        sound[7] = m_clipAmmo;
        sound[5] = 0;
        sound[6] = 0;
        sound[3] = 0;
        sound[4] = 0;
        AEMS_SendEvent(sound);
    }
    MakeNoise(m_pHolder, 0x34, m_pProperties->m_noise, 10);
    m_refireTime += (float)m_pProperties->m_refireRate;
}

// Refill the clip from the reserve (all of it with infinite ammo).
static inline void Refill(CWeapon* w) {
    w->m_bReloading = false;
    if (w->m_infiniteAmmo) {
        w->m_clipAmmo = w->m_pProperties->m_clipSize;
    } else {
        s16 clip = w->m_clipAmmo;
        s16 reserve = w->m_reserveAmmo;
        s16 size = w->m_pProperties->m_clipSize;
        if ((s16)(clip + reserve) < size) {
            w->m_clipAmmo = clip + reserve;
            w->m_reserveAmmo = 0;
        } else {
            w->m_reserveAmmo = reserve - (size - clip);
            w->m_clipAmmo = w->m_pProperties->m_clipSize;
        }
    }
}

// Script events on the holder: 0x19 out of ammo (reload; with the
// reserve), 0x15 ready to fire again.
void CWeapon::Update(float dt, bool paused) {
    m_fired = false;
    if (g_cheats.field_0x1 && m_reserveAmmo == 0) {
        m_reserveAmmo = 1;
    }
    switch (m_shootType) {
    case WEAPON_SHOOT_HOLD:
        if (m_refireTime <= 0.0f) {
            if (m_clipAmmo > 0 && !m_bReloading) {
                if (!paused) {
                    FireBullet();
                }
            } else if (!m_bReloading && !paused) {
                if (m_pHolder != NULL) {
                    m_pHolder->TriggerScriptEvent(0x19, (void*)m_reserveAmmo, false);
                }
                m_bReloading = true;
            }
        } else {
            m_refireTime -= dt;
        }
        if (m_refireTime <= 0.0f && m_pHolder != NULL && !paused) {
            m_pHolder->TriggerScriptEvent(0x15, NULL, true);
        }
        break;
    case WEAPON_SHOOT_NONE:
        if (m_refireTime > 0.0f) {
            m_refireTime -= dt;
            if (m_refireTime <= 0.0f && m_pHolder != NULL && !paused) {
                m_pHolder->TriggerScriptEvent(0x15, NULL, true);
            }
        }
        if (m_clipAmmo <= 0 && !m_bReloading && !paused) {
            if (m_pHolder != NULL) {
                m_pHolder->TriggerScriptEvent(0x19, (void*)m_reserveAmmo, false);
            }
            m_bReloading = true;
        }
        break;
    case WEAPON_SHOOT_ONCE:
        m_shootType = WEAPON_SHOOT_NONE;
        if (!paused) {
            FireBullet();
        }
        if (m_clipAmmo <= 0 && !paused) {
            if (m_pHolder != NULL) {
                m_pHolder->TriggerScriptEvent(0x19, (void*)m_reserveAmmo, false);
            }
            AddAmo(m_pProperties->m_clipSize);
            Refill(this);
        }
        break;
    }

    if (m_bDropped) {
        // Fall until a 1-unit probe below finds the floor, then lie on it.
        CCollision clsn;
        const IVolume* world = g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
        CVector3 pos(*(const CVector3*)m_tm.m[3]);
        CVector3 bottom(pos.x, pos.y, pos.z - 0.05f);
        CVector3 top(pos.x, pos.y, 1.0f + bottom.z);
        CLine3 line;
        line.SetSE(top, bottom);
        CVolFiber probe(line);
        if (world->TestCollision(probe, clsn, true) == CLSN_NONE) {
            m_tm.Translate(m_velocity);
            m_velocity.z = -0.005455351f * dt + m_velocity.z;
        } else {
            m_velocity.z = 0.99f - clsn.m_fraction;
            m_tm.Translate(m_velocity);
            m_velocity.z = 0.0f;
            CVector3 normal(clsn.m_pContacts->m_normal);
            CVector3 front(*(const CVector3*)m_tm.m[1]);
            CVector3 up;
            Cross(up, normal, front);
            Cross(front, up, normal);
            up.Normalize();
            front.Normalize();
            m_tm.SetFront(front);
            m_tm.SetRight(normal);
            m_tm.SetUp(up);
            m_bDropped = false;
        }
    }
    if (m_muzzleFlash) {
        m_flashShown = true;
        if (!m_fired) {
            m_flashTime += dt;
        }
    }
}

void CWeapon::Draw(CDrawContext& ctx) {
    if (m_pMesh != NULL && m_pHolder->AsPlayerObject() == NULL) {
        CStaticObject::Draw(ctx);
    }
}

int CWeapon::BeginUpdate(float dt) {
    if (m_bDropped) {
        ForceUpdateProximityTriggerStatus(&m_bsgo, true);
    }
    return CStaticObject::BeginUpdate(dt);
}

void CWeapon::DoneReloading() {
    Refill(this);
}

void CWeapon::ReloadSingle() {
    if (m_clipAmmo < m_pProperties->m_clipSize && m_reserveAmmo > 0) {
        m_clipAmmo++;
        m_reserveAmmo--;
    }
}

void CWeapon::StartReloading() {
    m_shootType = WEAPON_SHOOT_NONE;
    m_bReloading = true;
}

void CWeapon::Empty() {
    m_reserveAmmo = 0;
    m_clipAmmo = 0;
}

// Picking up ammo with an empty gun starts a reload.
void CWeapon::AddAmo(short count) {
    if (m_reserveAmmo + m_clipAmmo == 0) {
        m_bReloading = true;
    }
    m_reserveAmmo += count;
    s16 reserve = m_reserveAmmo;
    if (reserve > m_pProperties->m_maxAmmo) {
        reserve = m_pProperties->m_maxAmmo;
    }
    m_reserveAmmo = reserve;
}

void CWeapon::Shoot(EWeaponShootType type, float power, float fuse) {
    m_shootType = type;
    m_power = power;
    m_fuse = fuse;
}

CSprite* CWeapon::GetBulletSprite() const {
    return g_pBulletFactory->GetBulletSprite(m_pProperties->m_isProjectile != 0, m_pProperties->m_bulletType);
}

void CWeapon::Shutdown() {}

// The holder's collision id picks the bullets' (players 3/9, soldiers
// 4/10, tanks 6/11 for projectile/thrown).
void CWeapon::Init(EWeaponTypes type, WeaponProperties_struct* props, ISceneNode* holder, short ammo) {
    CStaticObject::Init();
    m_refireTime = 0.0f;
    m_shootType = WEAPON_SHOOT_NONE;
    m_fired = false;
    m_flashDuration = 4.0f;
    m_pHolder = holder;
    m_infiniteAmmo = false;
    m_pProperties = props;
    m_weaponType = type;
    m_pMesh = props->m_pMesh;
    m_flashShown = false;
    m_muzzleFlash = false;
    m_flashTime = 0.0f;
    m_bDropped = false;
    m_speedScale = 1.0f;
    if (ammo > m_pProperties->m_maxAmmo) {
        ammo = m_pProperties->m_maxAmmo;
    }
    m_reserveAmmo = ammo;
    Refill(this);
    m_aimOrigin.z = 0.0f;
    m_aimOrigin.y = 0.0f;
    m_aimOrigin.x = 0.0f;
    if (m_pHolder != NULL) {
        switch (m_pHolder->GetCollisionId()) {
        case 2:
        case 13:
        case 23:
            m_clsnId = m_pProperties->m_isProjectile != 0 ? 4 : 10;
            break;
        case 1:
            m_clsnId = m_pProperties->m_isProjectile != 0 ? 3 : 9;
            break;
        case 5:
            m_clsnId = m_pProperties->m_isProjectile != 0 ? 6 : 11;
            break;
        default:
            m_clsnId = -1;
            break;
        }
    }
    switch (m_weaponType) {
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x28:
        m_pMuzzleFlash = NULL;
        break;
    default:
        m_pMuzzleFlash = g_pStaticObjectMuzzleFlashNPC;
        break;
    }
    m_bsgo.m_proximity[1] = 0;
    m_bsgo.m_proximity[0] = 0;
    m_bsgo.m_proximity[3] = 0;
    m_bsgo.m_proximity[2] = 0;
    m_bsgo.m_object = NULL;
    m_bsgo.m_node = this;
}

CWeapon::~CWeapon() {}

CWeapon::CWeapon() {
    m_bsgo.m_object = NULL;
    m_bsgo.m_pNext = NULL;
    m_bsgo.m_pPrev = NULL;
    m_reserveAmmo = 0;
    m_clipAmmo = 0;
    m_refireTime = 0.0f;
    m_shootType = WEAPON_SHOOT_NONE;
    m_pProperties = NULL;
}

void* CWeapon::operator new[](unsigned long size, char* name) {
    return DWI_alloc(name, size, 0x400);
}
