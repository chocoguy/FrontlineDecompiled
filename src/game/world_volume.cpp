#include "game/world_volume.h"

#include "game/SClsnContact.h"
#include "game/animated_volume.h"
#include "game/box.h"
#include "game/capsule.h"
#include "game/cdbobject.h"
#include "game/collision.h"
#include "game/fiber.h"
#include "game/line.h"
#include "game/sphere.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define BIG_FLOAT 3.4028235e38f

typedef dwi::list<const CCDBObject*>::node SubVolumeNode;

// Tests `volume` against each sub volume on a copy of the collision. In
// nearest-hit mode the nearest hit wins; otherwise the worst result (the
// last of equals), stopping at the first penetration.
template <class T>
int CWorldVolume::TestCollisionWithSubVolumes(const T& volume, CCollision& collision,
                                              bool record) const {
    int worst = CLSN_NONE;
    for (const SubVolumeNode* n = m_subVolumes.m_head.next; n != &m_subVolumes.m_head; n = n->next) {
        const CCDBObject* sub = n->value;
        CCollision subCollision(collision);
        int result = sub->TestCollision(volume, subCollision, record);
        if (collision.m_flags & CLSN_FLAG_NEAREST) {
            if (result != CLSN_NONE && subCollision.m_fraction <= collision.m_fraction) {
                collision = subCollision;
                worst = result;
            }
        } else {
            if (result >= worst) {
                worst = result;
                collision = subCollision;
            }
            if (worst == CLSN_PENETRATING) {
                break;
            }
        }
    }
    return worst;
}

CWorldVolume::CWorldVolume(int capacity) {
    field_0x48 = 0;
    m_subVolumes.reserve(capacity, "World volume sub volumes");
}

CWorldVolume::~CWorldVolume() {}

// Recomputes the extents from the sub volumes (zero when there are none).
inline void CWorldVolume::UpdateExtents() {
    if (m_subVolumes.m_size != 0) {
        m_min.x = BIG_FLOAT;
        m_min.y = BIG_FLOAT;
        m_min.z = BIG_FLOAT;
        m_max.x = -m_min.x;
        m_max.y = -m_min.y;
        m_max.z = -m_min.z;
        for (SubVolumeNode* n = m_subVolumes.m_head.next; n != &m_subVolumes.m_head; n = n->next) {
            CVector3 min;
            CVector3 max;
            n->value->GetExtents(min, max);
            m_min.x = m_min.x < min.x ? m_min.x : min.x;
            m_min.y = m_min.y < min.y ? m_min.y : min.y;
            m_min.z = m_min.z < min.z ? m_min.z : min.z;
            m_max.x = m_max.x > max.x ? m_max.x : max.x;
            m_max.y = m_max.y > max.y ? m_max.y : max.y;
            m_max.z = m_max.z > max.z ? m_max.z : max.z;
        }
        return;
    }
    m_min.z = 0.0f;
    m_min.y = 0.0f;
    m_min.x = 0.0f;
    m_max.z = 0.0f;
    m_max.y = 0.0f;
    m_max.x = 0.0f;
}

void CWorldVolume::AddSubVolume(const CCDBObject& object) {
    m_subVolumes.push_back(&object);
    UpdateExtents();
}

void CWorldVolume::RemoveSubVolume(const CCDBObject& object) {
    for (SubVolumeNode* n = m_subVolumes.m_head.next; n != &m_subVolumes.m_head; n = n->next) {
        if (n->value == &object) {
            RemoveSubVolume(dwi::list_iter<const CCDBObject*>(n));
            return;
        }
    }
}

void CWorldVolume::RemoveSubVolume(dwi::list_iter<const CCDBObject*> it) {
    m_subVolumes.erase(it.m_node);
    UpdateExtents();
}

// Double dispatch: swap the roles and let the other volume test the world.
int CWorldVolume::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

int CWorldVolume::TestCollision(const CVolBox& box, CCollision& collision, bool record) const {
    return TestCollisionWithSubVolumes(box, collision, record);
}

int CWorldVolume::TestCollision(const CVolSphere& sphere, CCollision& collision, bool record) const {
    return TestCollisionWithSubVolumes(sphere, collision, record);
}

int CWorldVolume::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool record) const {
    return TestCollisionWithSubVolumes(capsule, collision, record);
}

int CWorldVolume::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    return TestCollisionWithSubVolumes(line, collision, record);
}

int CWorldVolume::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    return TestCollisionWithSubVolumes(fiber, collision, record);
}

int CWorldVolume::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    return TestCollisionWithSubVolumes(volume, collision, record);
}

int CWorldVolume::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

void CWorldVolume::GetExtents(CVector3& min, CVector3& max) const {
    min.Copy(m_min);
    max.Copy(m_max);
}

int CWorldVolume::TestCollision(const CCDBObject&, CCollision&, bool) const {
    return 0;
}

int CWorldVolume::TestCollision(const CWorldVolume&, CCollision&, bool) const {
    return 0;
}

int CWorldVolume::TestCollision(CVector3, CCollision&, bool) const {
    return 0;
}

int CWorldVolume::TestCollision(const CPlane&, CCollision&, bool) const {
    return 0;
}

int CWorldVolume::TestCollision(const CTriangle&, CCollision&, bool) const {
    return 0;
}
