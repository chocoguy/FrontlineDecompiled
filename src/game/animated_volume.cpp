#include "game/animated_volume.h"

#include <math.h>
#include "game/SClsnContact.h"
#include "game/anim_obj.h"
#include "game/collision.h"
#include "game/fiber.h"
#include "game/matrix.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

// The owner's bone matrices (CAnimObject + 0xF40, not typed yet).
#define OWNER_BONES(owner) ((const CMatrix*)((const u8*)(owner) + 0xF40))

void CAnimatedVolume::TransformedCopy(const IVolume& volume, const CMatrix& m) {
    const CAnimatedVolume& src = (const CAnimatedVolume&)volume;
    CVolSphere::TransformedCopy(volume, m);
    m_pData = src.m_pData;
    m_pMap = src.m_pMap;
    m_boxesValid = false;
    m_pOwner = src.m_pOwner;
}

int CAnimatedVolume::Create() const {
    return (int)new CAnimatedVolume;
}

int CAnimatedVolume::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision((const CVolSphere&)volume, collision, record);
}

// Double dispatch: swap the roles and let the other volume test this one.
int CAnimatedVolume::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CAnimatedVolume::TestCollision(const CWorldVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

// A fiber is a line that only reports its nearest hit.
int CAnimatedVolume::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return TestCollision(fiber.GetLine(), collision, record);
}

// Everything but lines only sees the bounding sphere.
int CAnimatedVolume::TestCollision(const CTriangle& triangle, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision(triangle, collision, record);
}

int CAnimatedVolume::TestCollision(const CPlane& plane, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision(plane, collision, record);
}

int CAnimatedVolume::TestCollision(CVector3 point, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision(point, collision, record);
}

int CAnimatedVolume::TestCollision(const CCDBObject& object, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)object).TestCollision(*this, collision, record);
}

int CAnimatedVolume::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision(capsule, collision, record);
}

int CAnimatedVolume::TestCollision(const CVolSphere& sphere, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision(sphere, collision, record);
}

int CAnimatedVolume::TestCollision(const CVolBox& box, CCollision& collision, bool record) const {
    return CVolSphere::TestCollision(box, collision, record);
}

int CAnimatedVolume::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

// Lines (bullets) are tested against the bounding sphere, then each bone
// box. The hit goes to the highest-region box (by the location map), the
// nearest among equals; its location goes in the contact's polygon field.
int CAnimatedVolume::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    CCollision sphereCollision(collision);
    if (CVolSphere::TestCollision(line, sphereCollision, false) == CLSN_NONE) {
        if (record && collision.m_result < CLSN_NONE) {
            collision.m_result = CLSN_NONE;
        }
        return CLSN_NONE;
    }

    CCollision collisions[LOCATION_COLLISION_MAX_SHAPES];
    int count = 0;
    if (!m_boxesValid) {
        CMatrix world;
        m_pOwner->GetTMLocalToWorld(world);
        const CMatrix* bones = OWNER_BONES(m_pOwner);
        if (m_pData != NULL) {
            for (int i = 0; i < m_pData->numShapes; i++) {
                CMatrix m;
                m = bones[m_pData->shapes[i].bone];
                m.TibToMOHFL();
                CVolBox box;
                box.TransformedCopy(*(const IVolume*)m_pData->shapes[i].box, m);
                m_boxes[i].TransformedCopy(box, world);
            }
        }
        m_boxesValid = true;
    }

    float fractions[LOCATION_COLLISION_MAX_SHAPES];
    int hits[LOCATION_COLLISION_MAX_SHAPES];
    int* hit = hits;
    float* fraction = fractions;
    for (int i = 0; i < m_pData->numShapes; i++) {
        collisions[i] = collision;
        if (m_boxes[i].TestCollision(line, collisions[i], true) != CLSN_NONE) {
            count++;
            *hit++ = i;
            *fraction++ = collisions[i].m_fraction;
        }
    }

    float bestFraction = 1e30f;
    int best = -1;
    int bestRegion = 0;
    int bestLocation = 0;
    for (int k = 0; k < count; k++) {
        int index = hits[k];
        const CObjLocationCollisionMap::LocationInfo& info = m_pMap->m_info[index];
        if (info.valid) {
            bool take = false;
            if (info.region > bestRegion) {
                take = true;
            } else if (info.region == bestRegion && (float)fabs(fractions[k]) < bestFraction) {
                take = true;
            }
            if (take) {
                bestLocation = info.location;
                best = index;
                bestFraction = fractions[k];
                bestRegion = info.region;
            }
        }
    }

    int result;
    if (best == -1) {
        if (record && collision.m_result < CLSN_NONE) {
            collision.m_result = CLSN_NONE;
        }
        result = CLSN_NONE;
    } else {
        const CCollision& hitCollision = collisions[best];
        collision.m_fraction = hitCollision.m_fraction;
        collision.m_flags = (collision.m_flags & ~CLSN_FLAG_NEAREST) | (hitCollision.m_flags & CLSN_FLAG_NEAREST);
        result = hitCollision.m_result;
        if (record) {
            if (result > collision.m_result) {
                collision.m_result = result;
            }
            if (result == CLSN_TOUCHING) {
                const SClsnContact* first = hitCollision.m_pContacts;
                SClsnContact contact;
                contact.m_depth = first->m_depth;
                contact.m_normal.Copy(first->m_normal);
                contact.m_point.Copy(first->m_point);
                contact.m_pPoly = (const SClsnPoly*)bestLocation;
                collision.AddContact(contact);
            }
        }
    }
    return result;
}

void CAnimatedVolume::Set(CObjLocationCollisionData* data, CObjLocationCollisionMap* map) {
    m_pData = data;
    m_pMap = map;
    m_boxesValid = false;
}

CAnimatedVolume::~CAnimatedVolume() {}

CAnimatedVolume::CAnimatedVolume() {
    m_pData = NULL;
    m_boxesValid = false;
    m_pMap = NULL;
    m_pOwner = NULL;
}
