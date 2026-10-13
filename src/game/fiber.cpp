#include "game/fiber.h"

#include <math.h>
#include "game/SClsnContact.h"
#include "game/box.h"
#include "game/collision.h"
#include "game/matrix.h"
#include "game/plane.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define FIBER_EPSILON 1e-12f
#define LINE_HIT_TOLERANCE 0.001f

static inline int Miss(CCollision& collision, bool record) {
    if (record && collision.m_result < CLSN_NONE) {
        collision.m_result = CLSN_NONE;
    }
    return CLSN_NONE;
}

CVolFiber::CVolFiber(const CVolFiber& other) {
    m_line.m_start.Copy(other.m_line.m_start);
    m_line.m_end.Copy(other.m_line.m_end);
    m_line.m_dir.Copy(other.m_line.m_dir);
    m_line.m_length = other.m_line.m_length;
    m_line.m_lengthSq = other.m_line.m_lengthSq;
    m_line.m_lengthValid = other.m_line.m_lengthValid;
    m_line.m_lengthSqValid = other.m_line.m_lengthSqValid;
}

CVolFiber::CVolFiber(CVector3 start, CVector3 end) {
    m_line.SetSE(start, end);
}

CVolFiber::CVolFiber(const CLine3& line) {
    m_line.m_start.Copy(line.m_start);
    m_line.m_end.Copy(line.m_end);
    m_line.m_dir.Copy(line.m_dir);
    m_line.m_length = line.m_length;
    m_line.m_lengthSq = line.m_lengthSq;
    m_line.m_lengthValid = line.m_lengthValid;
    m_line.m_lengthSqValid = line.m_lengthSqValid;
}

CVolFiber::~CVolFiber() {}

int CVolFiber::Create() const {
    return (int)new CVolFiber;
}

void CVolFiber::TransformedCopy(const IVolume& volume, const CMatrix& m) {
    const CVolFiber& src = (const CVolFiber&)volume;
    m_line = src.m_line;
    CVector3 start;
    m.TransformPoint(start, src.m_line.m_start);
    CVector3 end;
    m.TransformPoint(end, src.m_line.m_end);
    m_line.SetSE(start, end);
}

void CVolFiber::Set(CVector3 start, CVector3 end) {
    m_line.SetSE(start, end);
}

const CLine3& CVolFiber::GetLine() const {
    return m_line;
}

// Double dispatch: swap the roles and let the other volume test a fiber.
int CVolFiber::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CVolBox& box, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)box).TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CVolSphere& sphere, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)sphere).TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)capsule).TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CCDBObject& object, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)object).TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolFiber::TestCollision(const CWorldVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

// The point counts as hit when it lies on the segment.
int CVolFiber::TestCollision(CVector3 point, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    CVector3 dir = m_line.m_dir;
    CVector3 start = m_line.m_start;
    float dx = point.x - start.x;
    float dy = point.y - start.y;
    float dz = point.z - start.z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float along = dir.x * dx + dir.y * dy + dir.z * dz;
    if (along < dist - FIBER_EPSILON || along > FIBER_EPSILON + dist) {
        return Miss(collision, record);
    }
    float length = m_line.GetLength();
    if (along < 0.0f || along > length) {
        return Miss(collision, record);
    }
    if (record) {
        if (collision.m_result < CLSN_TOUCHING) {
            collision.m_result = CLSN_TOUCHING;
        }
        if (length > FIBER_EPSILON) {
            collision.m_fraction = along / length;
        } else {
            collision.m_fraction = 0.0f;
        }
        // Any direction perpendicular to the fiber.
        SClsnContact contact;
        if ((float)fabs(dir.x) < (float)fabs(dir.y)) {
            contact.m_normal.Cross(dir, CVector3(1.0f, 0.0f, 0.0f));
        } else if ((float)fabs(dir.y) < (float)fabs(dir.z)) {
            contact.m_normal.Cross(dir, CVector3(0.0f, 1.0f, 0.0f));
        } else {
            contact.m_normal.Cross(dir, CVector3(0.0f, 0.0f, 1.0f));
        }
        contact.m_depth = along;
        contact.m_point.Copy(point);
        contact.m_pPoly = NULL;
        collision.AddContact(contact);
    }
    return CLSN_TOUCHING;
}

int CVolFiber::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return TestCollision(fiber.m_line, collision, record);
}

// Two segments hit when their closest points (nearly) coincide.
int CVolFiber::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    CVector3 a0 = m_line.m_start;
    CVector3 a1 = m_line.m_end;
    CVector3 b0 = line.m_start;
    CVector3 b1 = line.m_end;
    float d1x = a1.x - a0.x;
    float d1y = a1.y - a0.y;
    float d1z = a1.z - a0.z;
    float d2x = b1.x - b0.x;
    float d2y = b1.y - b0.y;
    float d2z = b1.z - b0.z;
    float rx = a0.x - b0.x;
    float ry = a0.y - b0.y;
    float rz = a0.z - b0.z;
    if ((float)fabs(d1x) < FIBER_EPSILON && (float)fabs(d1y) < FIBER_EPSILON &&
        (float)fabs(d1z) < FIBER_EPSILON) {
        return Miss(collision, record);
    }
    if ((float)fabs(d2x) < FIBER_EPSILON && (float)fabs(d2y) < FIBER_EPSILON &&
        (float)fabs(d2z) < FIBER_EPSILON) {
        return Miss(collision, record);
    }
    float d12 = d2x * d1x + d2y * d1y + d2z * d1z;
    float d22 = d2x * d2x + d2y * d2y + d2z * d2z;
    float denom = d22 * (d1x * d1x + d1y * d1y + d1z * d1z) - d12 * d12;
    if ((float)fabs(denom) < FIBER_EPSILON) {
        return Miss(collision, record);
    }
    float inv = 1.0f / denom;
    float r2 = rx * d2x + ry * d2y + rz * d2z;
    float s = inv * (r2 * d12 - d22 * (rx * d1x + ry * d1y + rz * d1z));
    if (s < 0.0f || s > 1.0f) {
        return Miss(collision, record);
    }
    // (As in the original: scaled by 1/denom rather than 1/d22.)
    float t = inv * (d12 * s + r2);
    if (t < 0.0f || t > 1.0f) {
        return Miss(collision, record);
    }

    CVector3 start = m_line.m_start;
    CVector3 lineStart = line.m_start;
    CVector3 p;
    p.x = s * d1x;
    p.y = s * d1y;
    p.z = s * d1z;
    p.x = p.x + start.x;
    p.y = p.y + start.y;
    p.z = p.z + start.z;
    p.x = p.x - (t * d2x + lineStart.x);
    p.y = p.y - (t * d2y + lineStart.y);
    p.z = p.z - (t * d2z + lineStart.z);
    if ((float)fabs(p.x) > LINE_HIT_TOLERANCE || (float)fabs(p.y) > LINE_HIT_TOLERANCE ||
        (float)fabs(p.z) > LINE_HIT_TOLERANCE) {
        return Miss(collision, record);
    }
    if (record) {
        if (collision.m_result < CLSN_TOUCHING) {
            collision.m_result = CLSN_TOUCHING;
        }
        // (As in the original, `p` is the closest points' difference here,
        // not the hit point.)
        SClsnContact contact;
        CVector3 fromStart;
        start = m_line.m_start;
        fromStart.x = p.x - start.x;
        fromStart.y = p.y - start.y;
        fromStart.z = p.z - start.z;
        contact.m_depth = fromStart.Length();
        collision.m_fraction = s;
        CVector3 dir = m_line.m_dir;
        CVector3 lineDir = line.m_dir;
        contact.m_normal.Cross(dir, lineDir);
        contact.m_point.Copy(p);
        contact.m_pPoly = NULL;
        collision.AddContact(contact);
    }
    return CLSN_TOUCHING;
}

int CVolFiber::TestCollision(const CPlane& plane, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    float t;
    CVector3 point;
    bool crosses = plane.GetIntersection(m_line, t, &point);
    bool inRange = false;
    if (t >= 0.0f && t <= 1.0f) {
        inRange = true;
    }
    if (!(crosses & inRange)) {
        return Miss(collision, record);
    }
    collision.m_fraction = t;
    if (record) {
        if (collision.m_result < CLSN_TOUCHING) {
            collision.m_result = CLSN_TOUCHING;
        }
        SClsnContact contact;
        contact.m_depth = 0.0f;
        contact.m_normal.x = -plane.m_normal[0];
        contact.m_normal.y = -plane.m_normal[1];
        contact.m_normal.z = -plane.m_normal[2];
        contact.m_point.Copy(point);
        contact.m_pPoly = NULL;
        collision.AddContact(contact);
    }
    return CLSN_TOUCHING;
}

int CVolFiber::TestCollision(const CTriangle&, CCollision& collision, bool) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return 0;
}

bool CVolFiber::TestVisibility(CDrawContext& ctx) const {
    CVolBox box;
    CVector3 min;
    CVector3 max;
    GetExtents(min, max);
    // (The box is built from the end points, not the extents.)
    box.SetCorners(m_line.m_start, m_line.m_end);
    return box.TestVisibility(ctx);
}

void CVolFiber::GetExtents(CVector3& min, CVector3& max) const {
    if (m_line.m_start.x < m_line.m_end.x) {
        min.x = m_line.m_start.x;
        max.x = m_line.m_end.x;
    } else {
        max.x = m_line.m_start.x;
        min.x = m_line.m_end.x;
    }
    if (m_line.m_start.y < m_line.m_end.y) {
        min.y = m_line.m_start.y;
        max.y = m_line.m_end.y;
    } else {
        max.y = m_line.m_start.y;
        min.y = m_line.m_end.y;
    }
    if (m_line.m_start.z < m_line.m_end.z) {
        min.z = m_line.m_start.z;
        max.z = m_line.m_end.z;
    } else {
        max.z = m_line.m_start.z;
        min.z = m_line.m_end.z;
    }
}
