// Hearing volumes: spheres around moving nodes that pick up bullets flying
// past (bullet whizz sounds). With the bullet-sparks cheat, hits spawn a
// particle effect reflected off the sphere instead of passing through.
#include "game/hearingvolume.h"

#include "game/IMovingSceneNode.h"
#include "game/bullet.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/particlesystem.h"
#include "game/Moh2.h"

CHearingVolume::~CHearingVolume() {}

CHearingVolume::CHearingVolume() {
    m_pOwner = NULL;
}

void CHearingVolume::Init(IMovingSceneNode* owner, EClsnId id, float radius, float height,
                          Callback callback) {
    m_tm.Ident();
    m_localSphere.SetCenter(CVector3(0.0f, 0.0f, height));
    m_localSphere.SetRadius(radius);
    m_pOwner = owner;
    m_collisionId = id;
    m_callback = callback;
    m_sceneLinks[1] = 0;
    m_sceneLinks[0] = 0;
    g_scene.Add(*this);
}

void CHearingVolume::GetTMLocalToWorld(CMatrix& tm) const {
    tm = m_tm;
}

void CHearingVolume::GetPosition(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[3];
}

void CHearingVolume::GetRightward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[0];
}

void CHearingVolume::GetForward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[1];
}

void CHearingVolume::GetUpward(CVector3& v) const {
    v = *(const CVector3*)m_tm.m[2];
}

const IVolume* CHearingVolume::GetLocalBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_localSphere;
}

const IVolume* CHearingVolume::GetWorldBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_worldSphere;
}

int CHearingVolume::BeginUpdate(float) {
    if (m_pOwner != NULL) {
        m_pOwner->GetTMLocalToWorld(m_tm);
        m_lastPos = *(const CVector3*)m_tm.m[3];
    }
}

int CHearingVolume::AttemptUpdate(float dt) {
    if (m_pOwner != NULL) {
        CVector3 vel = m_pOwner->GetWorldLinearVelocity();
        CVector3 move;
        move.x = dt * vel.x;
        move.y = dt * vel.y;
        move.z = dt * vel.z;
        m_tm.SetPos(m_lastPos);
        m_tm.Translate(move);
    }
    m_worldSphere.TransformedCopy(m_localSphere, m_tm);
    CVector3 min;
    CVector3 max;
    m_worldSphere.GetExtents(min, max);
    SetExtents(min, max);
}

int CBullet::Penetrate(const CCollision&) {}

void CBullet::SetExclusionPair(ISceneNode*) {}

int CHearingVolume::CommitUpdate() {
    m_lastPos = *(const CVector3*)m_tm.m[3];
}

void CHearingVolume::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    if (!g_cheats.field_0x8) {
        if (bullet->AsProjectile() != NULL) {
            m_callback(m_pOwner, bullet);
            bullet->SetExclusionPair(this);
            bullet->Penetrate(clsn);
            g_scene.ExcludeCollision(*clsn.m_pNode0, *clsn.m_pNode1, -1);
        }
        return;
    }

    const SClsnContact* contact = clsn.m_pContacts;
    CParticleSystem* ps = CParticleSystem::Create(0x59E08587);
    if (ps != NULL) {
        static CVector3 vRight(1.0f, 0.0f, 0.0f);
        CMatrix m;
        m.Ident();

        // Up is the bullet's direction reflected off the contact normal.
        CVector3 dir;
        bullet->GetForward(dir);
        CVector3 up;
        float d = -2.0f * (contact->m_normal.z * dir.z +
                           (contact->m_normal.x * dir.x + contact->m_normal.y * dir.y));
        up.x = d * contact->m_normal.x;
        up.y = d * contact->m_normal.y;
        up.z = d * contact->m_normal.z;
        up.x = up.x + dir.x;
        up.y = up.y + dir.y;
        up.z = up.z + dir.z;
        up.Normalize();

        CVector3 front;
        CVector3 right;
        if ((float)fabs(up.z * vRight.z + (up.x * vRight.x + up.y * vRight.y)) <= 0.99999f) {
            front.Cross(up, vRight);
            front.Normalize();
            right.Cross(front, up);
        } else {
            CVector3 z(0.0f, 0.0f, 1.0f);
            right.Cross(up, z);
            right.Normalize();
            front.Cross(up, right);
        }
        m.SetRight(right);
        m.SetFront(front);
        m.SetUp(up);
        m.SetPos(contact->m_point);
        m.Orthonormalize();
        ps->SetLocalToWorld(m);
    }
    m_callback(m_pOwner, bullet);
}

void PlayBulletWhizBys(IMovingSceneNode*, CBullet* bullet) {
    CVector3 pos;
    CVector3 dir;
    bullet->GetPosition(pos);
    bullet->GetForward(dir);
    PlayBulletWhiz(pos, dir, g_scene.GetPlayer(0));
}

void CHearingVolume::SetCollisionId(EClsnId id) {
    m_collisionId = id;
}

int CHearingVolume::GetCollisionId() const {
    return m_collisionId;
}
