#include "game/csg_volume.h"

#include "game/SClsnContact.h"
#include "game/collision.h"
#include "game/hierobject.h"
#include "game/vector.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define MAX_SUB_VOLUMES 20
#define BIG_FLOAT 3.4028235e38f

// Tests `volume` against each sub volume, each on a fresh collision. Any
// penetration ends the test; touching hits go to CheckContacts.
template <class T>
int CCSGVolume::TestSubVolumes(const T& volume, CCollision& collision, bool record) const {
    float fractions[MAX_SUB_VOLUMES];
    int hits[MAX_SUB_VOLUMES];
    CCollision collisions[MAX_SUB_VOLUMES];
    CCollision* sub = collisions;
    int* hit = hits;
    float* fraction = fractions;
    int count = 0;
    for (int i = 0; i < m_numSubVolumes; i++, sub++) {
        const IVolume* subVolume = m_pObject->GetSubVolume(i);
        if (subVolume != NULL) {
            int result = subVolume->TestCollision(volume, *sub, true);
            if (result == CLSN_TOUCHING) {
                count++;
                *hit++ = i;
                *fraction++ = sub->m_fraction;
            } else if (result == CLSN_PENETRATING) {
                return CLSN_PENETRATING;
            }
        }
    }
    return CheckContacts(hits, fractions, collisions, count, collision, record);
}

CCSGVolume::CCSGVolume() {
    m_pObject = NULL;
    m_numSubVolumes = 0;
}

CCSGVolume::~CCSGVolume() {}

void CCSGVolume::Init(int numSubVolumes, CHierObject* object) {
    m_numSubVolumes = numSubVolumes;
    m_pObject = object;
}

int CCSGVolume::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    return TestSubVolumes(line, collision, record);
}

int CCSGVolume::CheckContacts(int* hits, float* fractions, CCollision* collisions, int count,
                              CCollision& collision, bool record) const {
    float nearest = BIG_FLOAT;
    int nearestIndex = -1;
    for (int i = 0; i < count; i++) {
        if (fractions[i] < nearest) {
            nearest = fractions[i];
            nearestIndex = hits[i];
        }
    }
    int result;
    if (nearest == BIG_FLOAT) {
        if (record && collision.m_result < CLSN_NONE) {
            collision.m_result = CLSN_NONE;
        }
        result = CLSN_NONE;
    } else {
        CCollision& best = collisions[nearestIndex];
        collision.m_fraction = best.m_fraction;
        collision.m_flags = (collision.m_flags & ~CLSN_FLAG_NEAREST) | (best.m_flags & CLSN_FLAG_NEAREST);
        result = best.m_result;
        if (record) {
            // Every hit at the nearest fraction adds its first contact, with
            // the sub-volume index in the polygon field. (As in the original,
            // each is copied into the nearest hit's slot first.)
            for (int i = 0; i < count; i++) {
                if (nearest == fractions[i]) {
                    int index = hits[i];
                    best = collisions[index];
                    if (result > collision.m_result) {
                        collision.m_result = result;
                    }
                    if (result == CLSN_TOUCHING) {
                        const SClsnContact* first = best.m_pContacts;
                        SClsnContact contact;
                        contact.m_depth = first->m_depth;
                        contact.m_normal.Copy(first->m_normal);
                        contact.m_point.Copy(first->m_point);
                        contact.m_pPoly = ADDR_TO_PTR(const SClsnPoly, index);
                        collision.AddContact(contact);
                    }
                }
            }
        }
    }
    return result;
}

// Double dispatch: swap the roles and let the other volume test the CSG.
int CCSGVolume::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

int CCSGVolume::TestCollision(const CVolBox& box, CCollision& collision, bool record) const {
    return TestSubVolumes(box, collision, record);
}

int CCSGVolume::TestCollision(const CVolSphere& sphere, CCollision& collision, bool record) const {
    return TestSubVolumes(sphere, collision, record);
}

int CCSGVolume::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool record) const {
    return TestSubVolumes(capsule, collision, record);
}

int CCSGVolume::TestCollision(const CCDBObject& object, CCollision& collision, bool record) const {
    return TestSubVolumes(object, collision, record);
}

int CCSGVolume::TestCollision(CVector3 point, CCollision& collision, bool record) const {
    return TestSubVolumes(point, collision, record);
}

int CCSGVolume::TestCollision(const CPlane& plane, CCollision& collision, bool record) const {
    return TestSubVolumes(plane, collision, record);
}

int CCSGVolume::TestCollision(const CTriangle& triangle, CCollision& collision, bool record) const {
    return TestSubVolumes(triangle, collision, record);
}

int CCSGVolume::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    return TestSubVolumes(fiber, collision, record);
}

int CCSGVolume::TestCollision(const CWorldVolume& volume, CCollision& collision, bool record) const {
    return TestSubVolumes(volume, collision, record);
}

int CCSGVolume::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    return TestSubVolumes(volume, collision, record);
}

// (Another CSG volume is tested through IVolume.)
int CCSGVolume::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    return TestSubVolumes((const IVolume&)volume, collision, record);
}

int CCSGVolume::Create() const {
    return (int)PTR_TO_ADDR(new CCSGVolume);
}

void CCSGVolume::GetExtents(CVector3& min, CVector3& max) const {
    min.x = BIG_FLOAT;
    min.y = BIG_FLOAT;
    min.z = BIG_FLOAT;
    max.x = -BIG_FLOAT;
    max.y = -BIG_FLOAT;
    max.z = -BIG_FLOAT;
    for (int i = 0; i < m_numSubVolumes; i++) {
        const IVolume* sub = m_pObject->GetSubVolume(i);
        if (sub != NULL) {
            CVector3 subMin;
            CVector3 subMax;
            sub->GetExtents(subMin, subMax);
            if (subMin.x < min.x) {
                min.x = subMin.x;
            }
            if (subMin.y < min.y) {
                min.y = subMin.y;
            }
            if (subMin.z < min.z) {
                min.z = subMin.z;
            }
            if (subMax.x > max.x) {
                max.x = subMax.x;
            }
            if (subMax.y > max.y) {
                max.y = subMax.y;
            }
            if (subMax.z > max.z) {
                max.z = subMax.z;
            }
        }
    }
}

void CCSGVolume::TransformedCopy(const IVolume&, const CMatrix&) {}
