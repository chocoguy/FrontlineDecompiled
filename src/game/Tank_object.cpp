// Tanks: hierarchical objects with up to three weapons on their parts, tank
// AI, muzzle flashes and sparks when bullets hit them.
#include "game/Tank_object.h"

#include "game/weapon.h"
#include "game/weaponfactory.h"
#include "game/mainloop.h"
#include "game/AIFilter.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/bullet.h"
#include "game/particlesystem.h"
#include "game/MathFun.h"
#include "string.h"

#define HARDPOINT_MUZZLE 0xE6E804D5
#define WEAPON_CRC_NONE 0xFB9630C9

void CTankObject::VerifyAIDataSize() {}

CTankObject::CTankObject() {
    memset(m_weaponSlot, -1, sizeof(m_weaponSlot));
    memset(m_weapons, 0, sizeof(m_weapons));
    m_lookPart = 0;
    m_pathNodeTimer = 0;
    m_collisionDamage = 1;
}

CTankObject::CTankObject(CStaticMesh* mesh, int numAnimFrames, int animFrameTime,
                         int numMotionFrames, int motionFrameTime, int motionRelative,
                         CStaticObject::MotionFrame* motion)
    : CHierObject(mesh, numAnimFrames, animFrameTime, numMotionFrames, motionFrameTime,
                  motionRelative, motion) {
    memset(m_weaponSlot, -1, sizeof(m_weaponSlot));
    memset(m_weapons, 0, sizeof(m_weapons));
    m_lookPart = 0;
    m_pathNodeTimer = 0;
    m_collisionDamage = 1;
}

void CTankObject::InitAI() {
    m_ai.Init(this);
}

void CTankObject::CleanUp() {
    g_aigAIFilterGlobalObject.EnableAllAStarPathNodes();
    m_ai.CleanUp();
    for (int i = 0; i < 3; i++) {
        if (m_weapons[i] != NULL) {
            g_WeaponFactory.DestroyWeapon(m_weapons[i]);
        }
    }
    CHierObject::CleanUp();
}

CTankObject::~CTankObject() {}

int CTankObject::BeginUpdate(float dt) {
    CHierObject::BeginUpdate(dt);
    for (int i = 0; i < 3; i++) {
        if (m_weapons[i] != NULL) {
            m_weapons[i]->Update(dt, false);
        }
    }
    m_ai.PreUpdate();
}

// Every 60 frames the A* path nodes under the tank are disabled afresh.
void CTankObject::UpdateAI(float dt) {
    m_ai.Update(dt);
    if (m_pathNodeTimer <= 0) {
        g_aigAIFilterGlobalObject.EnableAllAStarPathNodes();
        g_aigAIFilterGlobalObject.DisableCollidingAStarPathNodes(
            GetWorldBoundingVolume((ISceneNode::EVolumeType)0));
        m_pathNodeTimer = 60;
        return;
    }
    m_pathNodeTimer = m_pathNodeTimer - (int)dt;
}

int CTankObject::AttemptUpdate(float dt) {
    return CHierObject::AttemptUpdate(dt);
}

int CTankObject::CommitUpdate() {
    return CHierObject::CommitUpdate();
}

// Bullets throw sparks off the armour.
void CTankObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    if (bullet->AsProjectile() != NULL) {
        const SClsnContact* contact = clsn.m_pContacts;
        CMatrix m;
        m.Ident();
        m.SetPos(contact->m_point);
        if (MathFunTestPercent(MathFunGetRandomPercent(), 30)) {
            CParticleSystem* ps = CParticleSystem::Create(0xE9AC3F2E);
            if (ps != NULL) {
                ps->SetLocalToWorld(m);
            }
        }
        for (int i = 0; i < 2; i++) {
            CParticleSystem* ps;
            if (MathFunTestPercent(MathFunGetRandomPercent(), 65)) {
                ps = CParticleSystem::Create(0xACE69E6E);
            } else {
                ps = CParticleSystem::Create(0xC74092F9);
            }
            if (ps != NULL) {
                ps->SetLocalToWorld(m);
            }
        }
    }
    CStaticObject::HandleBulletCollision(bullet, clsn);
}

// Draws the parts, then the muzzle flashes of the weapons that are firing.
void CTankObject::Draw(CDrawContext& ctx) {
    CHierObject::Draw(ctx);
    for (int i = 0; i <= 7; i++) {
        int slot = m_weaponSlot[i];
        if (slot < 0 || slot >= 3 || m_weapons[slot] == NULL) {
            continue;
        }
        IMovingSceneNode* flash = (IMovingSceneNode*)m_weapons[slot]->m_pMuzzleFlash;
        if (flash == NULL || !(u8)m_weapons[slot]->GetMuzzleFlashStatus()) {
            continue;
        }
        if (((int)m_weapons[slot]->m_flashTime & 3) != 0) {
            continue;
        }
        CMatrix tm;
        tm.Ident();
        CMatrix muzzle;
        muzzle.Ident();
        float angle = MathFunRandomReal(0.0f, 6.28f);
        CHierObject* part = (CHierObject*)GetSubObject(i);
        if (part != NULL) {
            tm = part->m_tm;
            part->GetAttachPointMatrix(HARDPOINT_MUZZLE, muzzle);
            tm.Multiply(muzzle, tm);
            tm.RotateY(angle);
            if (m_weapons[slot]->m_weaponType == 0x2C) {
                CMatrix scale;
                scale.Ident();
                scale.Scale(3.0f);
                tm.Multiply(scale, tm);
            }
            flash->SetTMLocalToWorld(tm);
            flash->Draw(ctx);
        }
    }
}

// The hull's matrix a unit up, or the look part's matrix.
void CTankObject::GetLookMatrix(CMatrix& tm) {
    if (m_lookPart != 0) {
        tm = ((CHierObject*)GetSubObject(m_lookPart))->m_tm;
        return;
    }
    tm = m_tm;
    CVector3 pos = *(const CVector3*)tm.m[3];
    pos.z = pos.z + 1.0f;
    tm.SetPos(pos);
}

int CTankObject::AddWeapon(int slot, int crc) {
    if ((u32)crc == WEAPON_CRC_NONE) {
        m_weapons[slot] = NULL;
        return 0;
    }
    m_weapons[slot] = g_WeaponFactory.CreateWeaponByCRC(crc, this, 999);
    if (m_weapons[slot] != NULL) {
        if (m_weapons[slot]->m_pProperties->m_isProjectile) {
            m_weapons[slot]->m_clsnId = 4;
        } else {
            m_weapons[slot]->m_clsnId = 10;
        }
        return 1;
    }
    return 0;
}

void CTankObject::MapWeaponSlot(int part, int slot) {
    m_weaponSlot[part] = slot;
}

// Fires part `part`'s weapon from the part's muzzle hard point.
void CTankObject::FireSubObject(int part) {
    CMatrix tm;
    CMatrix muzzle;
    int slot = m_weaponSlot[part];
    if (slot >= 0 && slot < 3) {
        CHierObject* obj = (CHierObject*)GetSubObject(part);
        m_weapons[slot]->m_muzzleFlash = 1;
        tm = obj->m_tm;
        obj->GetAttachPointMatrix(HARDPOINT_MUZZLE, muzzle);
        tm.Multiply(muzzle, tm);
        m_weapons[slot]->m_aimDir.Copy(*(const CVector3*)tm.m[1]);
        m_weapons[slot]->m_aimOrigin.Copy(*(const CVector3*)tm.m[3]);
        m_weapons[slot]->Shoot((EWeaponShootType)2, 1.0f, -1.0f);
    }
}

CTankObject* CTankObject::AsHierTankObject() {
    return this;
}

CAIDoodad* CTankObject::GetAIDoodad() {
    return &m_ai;
}

const CAIDoodad* CTankObject::GetAIDoodad() const {
    return &m_ai;
}
