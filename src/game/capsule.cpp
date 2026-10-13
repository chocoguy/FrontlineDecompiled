#include "game/capsule.h"

#include <math.h>
#include "game/SClsnContact.h"
#include "game/collision.h"
#include "game/fiber.h"
#include "game/matrix.h"
#include "game/plane.h"
#include "game/sphere.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define PARALLEL_EPSILON 0.99999f
#define ROOT_EPSILON 0.00001f

// Closest point on the capsule's axis segment to `point`.
static inline void ClosestAxisPoint(const CVolCapsule& capsule, CVector3 point, CVector3& out) {
    float t;
    capsule.m_line.GetProjection(point, out, t);
    if (t < 0.0f) {
        t = 0.0f;
        out = capsule.m_line.m_start;
    } else if (t > 1.0f) {
        t = 1.0f;
        out = capsule.m_line.m_end;
    }
}

CVolCapsule::~CVolCapsule() {}

int CVolCapsule::Create() const {
    return (int)new CVolCapsule;
}

void CVolCapsule::TransformedCopy(const IVolume& volume, const CMatrix& m) {
    const CVolCapsule& src = (const CVolCapsule&)volume;
    m_radius = src.m_radius;
    m_line = src.m_line;
    CVector3 start;
    m.TransformPoint(start, src.m_line.m_start);
    CVector3 end;
    m.TransformPoint(end, src.m_line.m_end);
    m_line.SetSE(start, end);
}

// Double dispatch: swap the roles and let the other volume test a capsule.
int CVolCapsule::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

int CVolCapsule::TestCollision(const CVolBox& box, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)box).TestCollision(*this, collision, record);
}

int CVolCapsule::TestCollision(const CVolSphere& sphere, CCollision& collision, bool record) const {
    CVector3 center = sphere.GetCenter();
    CVector3 closest;
    ClosestAxisPoint(*this, center, closest);
    CVector3 normal;
    normal.x = center.x - closest.x;
    normal.y = center.y - closest.y;
    normal.z = center.z - closest.z;
    float distSq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    float depth = sqrtf(distSq) - m_radius - sphere.GetRadius();
    int result = ClassifyContact(depth);
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            normal.Normalize();
            SClsnContact contact;
            contact.m_depth = depth;
            float r = m_radius;
            contact.m_point.x = closest.x + r * normal.x;
            contact.m_point.y = closest.y + r * normal.y;
            contact.m_point.z = closest.z + r * normal.z;
            contact.m_normal.Copy(normal);
            contact.m_pPoly = NULL;
            contact.m_pVolume = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolCapsule::TestCollision(const CVolCapsule& other, CCollision& collision, bool record) const {
    float radii = m_radius + other.m_radius;
    float radiiSq = radii * radii;
    bool alongAxis = true;
    bool beyondEnd = false;
    CLine3 otherLine = other.m_line;

    // Where the other capsule's end points project onto this axis.
    CVector3 startProj;
    float startT;
    m_line.GetProjection(otherLine.m_start, startProj, startT);
    CVector3 endProj;
    float endT;
    m_line.GetProjection(otherLine.m_end, endProj, endT);
    float s;
    float t;
    float distSq;
    m_line.GetSquaredDistanceBetweenSegments(other.m_line, s, t, &distSq);
    float length = m_line.GetLength();

    if (!(distSq <= radiiSq + CONTACT_EPSILON)) {
        return CLSN_NONE;
    }

    // Both ends beyond the same end of this axis: separated along it.
    float depth;
    float hi = 1.0f + CONTACT_EPSILON;
    if (startT > hi && endT > hi) {
        float a = (startT - 1.0f) * length;
        float b = (endT - 1.0f) * length;
        depth = a > b ? b : a;
        beyondEnd = true;
    } else {
        float lo = -CONTACT_EPSILON;
        if (startT < lo && endT < lo) {
            float a = -startT * length;
            float b = -endT * length;
            depth = a > b ? b : a;
        } else {
            alongAxis = false;
            depth = sqrtf(distSq) - (m_radius + other.m_radius);
        }
    }
    int result = ClassifyContact(depth);

    if (record && result != CLSN_NONE) {
        CVector3 normal;
        CVector3 point;
        if (alongAxis) {
            // (As in the original: the axis direction is not normalized, and
            // the point is the end away from the other capsule.)
            CVector3 dir = m_line.m_dir;
            normal = dir;
            if (beyondEnd) {
                normal.x *= -1.0f;
                normal.y *= -1.0f;
                normal.z *= -1.0f;
                point = m_line.m_start;
            } else {
                point = m_line.m_end;
            }
        } else {
            normal = otherLine.m_start;
            CVector3 start = otherLine.m_start;
            normal.x = normal.x - startProj.x;
            normal.y = normal.y - startProj.y;
            normal.z = normal.z - startProj.z;
            normal.Normalize();
            float r = m_radius;
            point.x = r * normal.x;
            point.y = r * normal.y;
            point.z = r * normal.z;
            point.x = point.x + startProj.x;
            point.y = point.y + startProj.y;
            point.z = point.z + startProj.z;
        }
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            SClsnContact contact;
            contact.m_depth = depth;
            contact.m_point.Copy(point);
            contact.m_normal.Copy(normal);
            contact.m_pPoly = NULL;
            contact.m_pVolume = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolCapsule::TestCollision(const CCDBObject& object, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)object).TestCollision(*this, collision, record);
}

// A fiber is a line that only reports its nearest hit.
int CVolCapsule::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    const CLine3& line = fiber.GetLine();
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return TestCollision(line, collision, record);
}

int CVolCapsule::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    return TestCollision((const CVolSphere&)volume, collision, record);
}

int CVolCapsule::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolCapsule::TestCollision(const CWorldVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolCapsule::TestCollision(CVector3 point, CCollision& collision, bool record) const {
    CVector3 closest;
    ClosestAxisPoint(*this, point, closest);
    CVector3 normal;
    normal.x = point.x - closest.x;
    normal.y = point.y - closest.y;
    normal.z = point.z - closest.z;
    float distSq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    float depth = sqrtf(distSq) - m_radius;
    int result = ClassifyContact(depth);
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            normal.Normalize();
            SClsnContact contact;
            contact.m_depth = depth;
            float r = m_radius;
            contact.m_point.x = closest.x + r * normal.x;
            contact.m_point.y = closest.y + r * normal.y;
            contact.m_point.z = closest.z + r * normal.z;
            contact.m_normal.Copy(normal);
            contact.m_pPoly = NULL;
            contact.m_pVolume = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

// Magic Software's line-capsule intersection (Eberly): the parameters of up
// to two crossings of the capsule surface, along `line`'s direction.
static int FindIntersection(const CLine3& line, const CVolCapsule& capsule, float* t) {
    // A frame (u, v, w) with w along the capsule axis.
    CVector3 w = capsule.m_line.m_dir;
    CVector3 origin = line.m_start;
    CVector3 lineDir = line.m_dir;
    float wLength = w.Normalize();
    float ux, uy, uz;
    if ((float)fabs(w.x) >= (float)fabs(w.y)) {
        float inv = 1.0f / sqrtf(w.x * w.x + w.z * w.z);
        uy = 0.0f;
        ux = inv * -w.z;
        uz = inv * w.x;
    } else {
        float inv = 1.0f / sqrtf(w.y * w.y + w.z * w.z);
        ux = 0.0f;
        uy = inv * w.z;
        uz = inv * -w.y;
    }
    float vx = w.y * uz - w.z * uy;
    float vy = w.z * ux - w.x * uz;
    float vz = w.x * uy - w.y * ux;

    // The line in that frame.
    float dx = ux * lineDir.x + uy * lineDir.y + uz * lineDir.z;
    float dy = vx * lineDir.x + vy * lineDir.y + vz * lineDir.z;
    float dz = w.x * lineDir.x + w.y * lineDir.y + w.z * lineDir.z;
    float dLength = sqrtf(dx * dx + dy * dy + dz * dz);
    if (dLength != 0.0f) {
        float inv = 1.0f / dLength;
        dx *= inv;
        dy *= inv;
        dz *= inv;
    }
    float absDz = fabs(dz);
    float invDLength = 1.0f / dLength;
    CVector3 base = capsule.m_line.m_start;
    float radius = capsule.m_radius;
    float diffY = origin.y - base.y;
    float radiusSq = radius * radius;
    float diffX = origin.x - base.x;
    float diffZ = origin.z - base.z;
    float pz = w.x * diffX + w.y * diffY + w.z * diffZ;
    float py = vx * diffX + vy * diffY + vz * diffZ;
    float px = ux * diffX + uy * diffY + uz * diffZ;

    if (absDz >= PARALLEL_EPSILON) {
        // Parallel to the axis: through both end caps or not at all.
        float discr = radiusSq - (py * py + px * px);
        if (discr >= 0.0f) {
            float root = sqrtf(discr);
            t[0] = invDLength * -(root + pz);
            t[1] = invDLength * (root + (wLength - pz));
            return 2;
        }
        return 0;
    }

    // The infinite cylinder.
    float a = dy * dy + dx * dx;
    float b = py * dy + px * dx;
    float c = (py * py + px * px) - radiusSq;
    float discr = b * b - a * c;
    if (discr < 0.0f) {
        return 0;
    }
    int count = 0;
    if (discr > ROOT_EPSILON) {
        float root = sqrtf(discr);
        float inv = 1.0f / a;
        float tt = inv * (-b - root);
        float z = tt * dz + pz;
        if (z >= 0.0f && z <= wLength) {
            count = 1;
            t[0] = tt * invDLength;
        }
        tt = inv * (-b + root);
        z = tt * dz + pz;
        if (z >= 0.0f && z <= wLength) {
            t[count++] = tt * invDLength;
        }
        if (count == 2) {
            return 2;
        }
    } else {
        float tt = -b / a;
        float z = tt * dz + pz;
        if (z >= 0.0f && z <= wLength) {
            t[0] = tt * invDLength;
            return 1;
        }
    }

    // The cap sphere at the axis start.
    b = pz * dz + b;
    c = pz * pz + c;
    discr = b * b - c;
    if (discr > ROOT_EPSILON) {
        float root = sqrtf(discr);
        float tt = -b - root;
        if (tt * dz + pz <= 0.0f) {
            t[count++] = tt * invDLength;
            if (count == 2) {
                return 2;
            }
        }
        tt = -b + root;
        if (tt * dz + pz <= 0.0f) {
            t[count++] = tt * invDLength;
            if (count == 2) {
                return 2;
            }
        }
    } else if (discr >= 0.0f) {
        float tt = -b;
        if (tt * dz + pz <= 0.0f) {
            t[count++] = tt * invDLength;
            if (count == 2) {
                return 2;
            }
        }
    }

    // The cap sphere at the axis end.
    b = b - wLength * dz;
    c = wLength * (wLength - 2.0f * pz) + c;
    discr = b * b - c;
    if (discr > ROOT_EPSILON) {
        float root = sqrtf(discr);
        float tt = -b - root;
        if (tt * dz + pz >= wLength) {
            t[count++] = tt * invDLength;
            if (count == 2) {
                return 2;
            }
        }
        tt = -b + root;
        if (tt * dz + pz >= wLength) {
            t[count++] = tt * invDLength;
            if (count == 2) {
                return 2;
            }
        }
    } else if (discr >= 0.0f) {
        float tt = -b;
        if (tt * dz + pz >= wLength) {
            t[count++] = tt * invDLength;
            if (count == 2) {
                return 2;
            }
        }
    }
    return count;
}

int CVolCapsule::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    if (m_line.IsDegenerate()) {
        CVolSphere sphere(m_line.m_start, m_radius);
        return sphere.TestCollision(line, collision, record);
    }

    float hits[2];
    float t;
    switch (FindIntersection(line, *this, hits)) {
    case 0:
        t = -1.0f;
        break;
    case 1:
        t = hits[0];
        break;
    case 2:
        if (hits[0] < hits[1] && hits[0] >= 0.0f) {
            t = hits[0];
        } else {
            t = hits[1];
        }
        break;
    }
    if (t < 0.0f || t > 1.0f) {
        if (record && collision.m_result < CLSN_NONE) {
            collision.m_result = CLSN_NONE;
        }
        return CLSN_NONE;
    }

    collision.m_fraction = t;
    CVector3 dir = line.m_dir;
    CVector3 start = line.m_start;
    CVector3 hit;
    hit.x = t * dir.x;
    hit.y = t * dir.y;
    hit.z = t * dir.z;
    hit.x = hit.x + start.x;
    hit.y = hit.y + start.y;
    hit.z = hit.z + start.z;

    int result;
    float distSq;
    if (collision.m_flags & CLSN_FLAG_NEAREST) {
        distSq = 0.0f;
        result = CLSN_TOUCHING;
    } else {
        // Touching only when the hit is the segment's end point.
        CVector3 end = line.m_end;
        CVector3 diff;
        diff.x = hit.x - end.x;
        diff.y = hit.y - end.y;
        diff.z = hit.z - end.z;
        distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        int r;
        if (-distSq > CONTACT_EPSILON_SQRD) {
            r = CLSN_NONE;
        } else if (distSq > CONTACT_EPSILON_SQRD) {
            r = CLSN_PENETRATING;
        } else {
            r = CLSN_TOUCHING;
        }
        result = r;
        if (r != CLSN_TOUCHING) {
            result = CLSN_PENETRATING;
        }
    }
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            CVector3 closest;
            ClosestAxisPoint(*this, hit, closest);
            CVector3 normal;
            normal.x = hit.x - closest.x;
            normal.y = hit.y - closest.y;
            normal.z = hit.z - closest.z;
            normal.Normalize();
            SClsnContact contact;
            contact.m_depth = -sqrtf(distSq);
            contact.m_normal.Copy(normal);
            contact.m_point.Copy(hit);
            contact.m_pPoly = NULL;
            contact.m_pVolume = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolCapsule::TestCollision(const CPlane& plane, CCollision& collision, bool record) const {
    CVector3 start = m_line.m_start;
    CVector3 end = m_line.m_end;
    float nx = plane.m_normal[0];
    float ny = plane.m_normal[1];
    float nz = plane.m_normal[2];
    float d = plane.m_d;
    float radius = m_radius;
    float startDist = (start.x * nx + start.y * ny + start.z * nz) - d;
    float endDist = (end.x * nx + end.y * ny + end.z * nz) - d;
    if (startDist * endDist > radius * radius) {
        // Both ends on the same side: test the nearer end's sphere.
        if ((float)fabs(startDist) <= (float)fabs(endDist)) {
            CVolSphere sphere(m_line.m_start, radius);
            return sphere.TestCollision(plane, collision, record);
        }
        CVolSphere sphere(m_line.m_end, radius);
        return sphere.TestCollision(plane, collision, record);
    }
    if (record && collision.m_result < CLSN_PENETRATING) {
        collision.m_result = CLSN_PENETRATING;
    }
    return CLSN_PENETRATING;
}

int CVolCapsule::TestCollision(const CTriangle&, CCollision&, bool) const {
    return 0;
}

bool CVolCapsule::TestVisibility(CDrawContext&) const {
    return true;
}

void CVolCapsule::GetExtents(CVector3& min, CVector3& max) const {
    CVector3 start = m_line.m_start;
    CVector3 end = m_line.m_end;
    float r = m_radius + CONTACT_EPSILON;
    float a, b;
    a = start.x - r;
    b = end.x - r;
    min.x = a < b ? a : b;
    a = start.y - r;
    b = end.y - r;
    min.y = a < b ? a : b;
    a = start.z - r;
    b = end.z - r;
    min.z = a < b ? a : b;
    start = m_line.m_start;
    end = m_line.m_end;
    a = start.x + r;
    b = end.x + r;
    max.x = a > b ? a : b;
    a = start.y + r;
    b = end.y + r;
    max.y = a > b ? a : b;
    a = start.z + r;
    b = end.z + r;
    max.z = a > b ? a : b;
}
