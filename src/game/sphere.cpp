#include "game/sphere.h"

#include "game/CVector4.h"
#include "game/SClsnContact.h"
#include "game/camera.h"
#include "game/collision.h"
#include "game/draw_context.h"
#include "game/fiber.h"
#include "game/line.h"
#include "game/matrix.h"
#include "game/plane.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

// Line tests treat lines shorter than this (squared) as points.
#define LINE_EPSILON 0.00001f

// Raises the collision's worst result when recording; returns `result`.
static inline int Record(CCollision& collision, bool record, int result) {
    if (record && result > collision.m_result) {
        collision.m_result = result;
    }
    return result;
}

CVolSphere::~CVolSphere() {}

void CVolSphere::operator=(const CVolSphere& other) {
    m_radius = other.m_radius;
    m_center.Copy(other.m_center);
}

int CVolSphere::Create() const {
    return (int)PTR_TO_ADDR(new CVolSphere);
}

void CVolSphere::TransformedCopy(const IVolume& volume, const CMatrix& m) {
    const CVolSphere& src = (const CVolSphere&)volume;
    m.TransformPoint(m_center, src.m_center);
    m_radius = src.m_radius;
}

void CVolSphere::SetCenter(CVector3 center) {
    m_center.Copy(center);
}

void CVolSphere::SetRadius(float radius) {
    m_radius = radius;
}

CVector3 CVolSphere::GetCenter() const {
    return m_center;
}

float CVolSphere::GetRadius() const {
    return m_radius;
}

// Double dispatch: swap the roles and let the other volume test a sphere.
int CVolSphere::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

int CVolSphere::TestCollision(const CVolBox& box, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)box).TestCollision(*this, collision, record);
}

int CVolSphere::TestCollision(const CVolSphere& other, CCollision& collision, bool record) const {
    CVector3 normal;
    normal.x = other.m_center.x - m_center.x;
    normal.y = other.m_center.y - m_center.y;
    normal.z = other.m_center.z - m_center.z;
    float dist = normal.Normalize();
    float depth = dist - (m_radius + other.m_radius);
    int result = ClassifyContact(depth);
    if (record) {
        Record(collision, record, result);
        if (result == CLSN_TOUCHING) {
            SClsnContact contact;
            contact.m_depth = depth;
            contact.m_normal.Copy(normal);
            normal.x *= m_radius;
            normal.y *= m_radius;
            normal.z *= m_radius;
            contact.m_point.x = m_center.x + normal.x;
            contact.m_point.y = m_center.y + normal.y;
            contact.m_point.z = m_center.z + normal.z;
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolSphere::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    return TestCollision((const CVolSphere&)volume, collision, record);
}

int CVolSphere::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolSphere::TestCollision(const CWorldVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolSphere::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)capsule).TestCollision(*this, collision, record);
}

// A fiber is a line that only reports its nearest hit.
int CVolSphere::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return TestCollision(fiber.GetLine(), collision, record);
}

int CVolSphere::TestCollision(const CCDBObject& object, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)object).TestCollision(*this, collision, record);
}

int CVolSphere::TestCollision(CVector3 point, CCollision& collision, bool record) const {
    CVector3 normal;
    normal.x = point.x - m_center.x;
    normal.y = point.y - m_center.y;
    normal.z = point.z - m_center.z;
    float dist = normal.Normalize();
    float depth = dist - m_radius;
    int result = ClassifyContact(depth);
    if (record) {
        Record(collision, record, result);
        if (result == CLSN_TOUCHING) {
            SClsnContact contact;
            contact.m_depth = depth;
            contact.m_normal.Copy(normal);
            normal.x *= m_radius;
            normal.y *= m_radius;
            normal.z *= m_radius;
            contact.m_point.x = m_center.x + normal.x;
            contact.m_point.y = m_center.y + normal.y;
            contact.m_point.z = m_center.z + normal.z;
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolSphere::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    CVector3 dir = line.m_dir;
    CVector3 start = line.m_start;
    float dx = start.x - m_center.x;
    float dy = start.y - m_center.y;
    float dz = start.z - m_center.z;
    float radius = m_radius;
    float a = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    float c = dx * dx + dy * dy + dz * dz;
    float b = 2.0f * (dir.x * dx + dir.y * dy + dir.z * dz);
    CVector3 diff;
    diff.x = dx;
    diff.y = dy;
    diff.z = dz;

    if (a > LINE_EPSILON) {
        // Solve |start + t dir - center| = radius for the entry point.
        float rr = radius * radius;
        float disc = b * b - 4.0f * a * (c - rr);
        if (disc < 0.0f) {
            if (record && collision.m_result < CLSN_NONE) {
                collision.m_result = CLSN_NONE;
            }
            return CLSN_NONE;
        }
        float t;
        if (disc > LINE_EPSILON) {
            float root = sqrtf(disc);
            float inv = 0.5f / a;
            t = inv * (-b - root);
            if (t > 1.0f || inv * (-b + root) < 0.0f) {
                if (record && collision.m_result < CLSN_NONE) {
                    collision.m_result = CLSN_NONE;
                }
                return CLSN_NONE;
            }
            if (t < 0.0f) {
                // The line starts inside the sphere.
                if (collision.m_flags & CLSN_FLAG_NEAREST) {
                    t = 0.0f;
                } else {
                    if (record && collision.m_result < CLSN_PENETRATING) {
                        collision.m_result = CLSN_PENETRATING;
                    }
                    return CLSN_PENETRATING;
                }
            }
        } else {
            t = -b / (2.0f * a);
        }
        if (t < 0.0f || t > 1.0f) {
            if (record && collision.m_result < CLSN_NONE) {
                collision.m_result = CLSN_NONE;
            }
            return CLSN_NONE;
        }

        collision.m_fraction = t;
        CVector3 hit;
        hit.x = t * dir.x;
        hit.y = t * dir.y;
        hit.z = t * dir.z;
        CVector3 lineStart = line.m_start;
        hit.x = hit.x + lineStart.x;
        hit.y = hit.y + lineStart.y;
        hit.z = hit.z + lineStart.z;

        int result;
        float distSq;
        if (collision.m_flags & CLSN_FLAG_NEAREST) {
            distSq = 0.0f;
            result = CLSN_TOUCHING;
        } else {
            // Touching only when the hit is the segment's end point.
            CVector3 end = line.m_end;
            diff.x = end.x - hit.x;
            diff.y = end.y - hit.y;
            diff.z = end.z - hit.z;
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
            Record(collision, record, result);
            if (result == CLSN_TOUCHING) {
                CVector3 normal;
                normal.x = hit.x - m_center.x;
                normal.y = hit.y - m_center.y;
                normal.z = hit.z - m_center.z;
                normal.Normalize();
                SClsnContact contact;
                contact.m_depth = -sqrtf(distSq);
                contact.m_point.Copy(hit);
                contact.m_normal.Copy(normal);
                contact.m_pPoly = NULL;
                collision.AddContact(contact);
            }
        }
        return result;
    }

    // A degenerate line: test its start as a point.
    collision.m_fraction = 1.0f;
    float depth = sqrtf(c) - m_radius;
    int result = ClassifyContact(depth);
    if ((collision.m_flags & CLSN_FLAG_NEAREST) && result == CLSN_PENETRATING) {
        result = CLSN_TOUCHING;
    }
    if (record) {
        Record(collision, record, result);
        if (result == CLSN_TOUCHING) {
            SClsnContact contact;
            CVector3 end = line.m_end;
            contact.m_normal.Copy(diff);
            contact.m_depth = depth;
            contact.m_point.Copy(end);
            contact.m_normal.Normalize();
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolSphere::TestCollision(const CPlane& plane, CCollision& collision, bool record) const {
    CVector3 center = m_center;
    float dist = plane.m_normal[0] * center.x + plane.m_normal[1] * center.y +
                 plane.m_normal[2] * center.z - plane.m_d - m_radius;
    int result = ClassifyContact(dist);
    if (record) {
        Record(collision, record, result);
        if (result == CLSN_TOUCHING) {
            // The original never sets the contact's depth.
            SClsnContact contact;
            float d = -dist;
            float nx = plane.m_normal[0];
            float ny = plane.m_normal[1];
            float nz = plane.m_normal[2];
            contact.m_point.x = d * nx;
            contact.m_point.y = d * ny;
            contact.m_point.z = d * nz;
            contact.m_point.x = contact.m_point.x + m_center.x;
            contact.m_point.y = contact.m_point.y + m_center.y;
            contact.m_point.z = contact.m_point.z + m_center.z;
            contact.m_normal.x = -nx;
            contact.m_normal.y = -ny;
            contact.m_normal.z = -nz;
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolSphere::TestCollision(const CTriangle&, CCollision&, bool) const {
    return 0;
}

// True when the sphere lies entirely outside the plane.
static inline bool Outside(const CPlane& plane, CVector3 center, float radius, u32& reject,
                           u32 bit) {
    float dist = plane.m_normal[0] * center.x + plane.m_normal[1] * center.y +
                 plane.m_normal[2] * center.z - plane.m_d;
    if (dist + radius > 0.0f) {
        reject |= bit;
    }
    return dist - radius > 0.0f;
}

bool CVolSphere::TestVisibility(CDrawContext& ctx) const {
    const CCamera* camera = (const CCamera*)ctx.m_pCamera;
    CVector3 view;
    camera->m_worldToCamera.TransformPoint(view, m_center);

    // Screen rectangle from two corners of the view-space bounding square.
    CVector3 lo;
    lo.x = view.x - m_radius;
    lo.y = view.y - m_radius;
    lo.z = view.z;
    CVector3 hi;
    hi.x = m_radius + view.x;
    hi.y = m_radius + view.y;
    hi.z = view.z;
    CVector4 loClip;
    camera->m_projection.Transform(loClip, lo);
    CVector4 hiClip;
    camera->m_projection.Transform(hiClip, hi);
    u32 outside = 0;
    float invW = 1.0f / loClip.w;
    ctx.m_screenMin[0] = invW * loClip.x;
    ctx.m_screenMin[1] = invW * loClip.y;
    invW = 1.0f / hiClip.w;
    ctx.m_screenMax[0] = invW * hiClip.x;
    ctx.m_screenMax[1] = invW * hiClip.y;

    ctx.m_2dReject = 0;
    const CPlane* planes = camera->m_frustum.m_planes;
    if (Outside(planes[0], m_center, m_radius, ctx.m_2dReject, DRAW_REJECT_NEAR)) {
        outside |= DRAW_REJECT_NEAR;
    }
    if (Outside(planes[1], m_center, m_radius, ctx.m_2dReject, DRAW_REJECT_FAR)) {
        outside |= DRAW_REJECT_FAR;
    }
    if (Outside(planes[2], m_center, m_radius, ctx.m_2dReject, DRAW_REJECT_LEFT)) {
        outside |= DRAW_REJECT_LEFT;
    }
    if (Outside(planes[3], m_center, m_radius, ctx.m_2dReject, DRAW_REJECT_RIGHT)) {
        outside |= DRAW_REJECT_RIGHT;
    }
    if (Outside(planes[4], m_center, m_radius, ctx.m_2dReject, DRAW_REJECT_TOP)) {
        outside |= DRAW_REJECT_TOP;
    }
    if (Outside(planes[5], m_center, m_radius, ctx.m_2dReject, DRAW_REJECT_BOTTOM)) {
        outside |= DRAW_REJECT_BOTTOM;
    }
    return outside == 0;
}

void CVolSphere::GetExtents(CVector3& min, CVector3& max) const {
    float r = m_radius;
    min.x = m_center.x - r;
    min.y = m_center.y - r;
    min.z = m_center.z - r;
    max.x = m_center.x + r;
    max.y = m_center.y + r;
    max.z = m_center.z + r;
}
