// Collision-database geometry: database triangles against the moving
// volumes (capsule, sphere, box, line) and database boxes as volumes.
// The distance code follows Magic Software's (Eberly's) segment/point to
// triangle distance and box/triangle separating-axis tests.
#include "game/cdbgeom.h"
#include "game/capsule.h"
#include "game/sphere.h"
#include "game/animated_volume.h"
#include "game/fiber.h"
#include "game/plane.h"
#include "game/line.h"
#include "game/collision.h"
#include "game/SClsnContact.h"

#define CDB_FLT_MAX 3.4028235e38f

// The capsule test is big enough to exhaust MWCC's default inlining budget.
#pragma inline_max_total_size(30000)

// The original unit called these header inlines out of line, and other
// objects (box, player, ...) link against its weak copies; referencing
// them here makes MWCC emit those copies.
struct CDBGeomWeakRefs {
    float (CVector3::*length)() const;
    float (CVector3::*normalize)();
    void (CVector3::*dotDotDot)(const CVector3&, const CVector3&, const CVector3&, const CVector3&);
    void (CLine3::*setSD)(CVector3, CVector3);
};
CDBGeomWeakRefs g_cdbgeomWeakRefs = {
    &CVector3::Length,
    &CVector3::Normalize,
    &CVector3::DotDotDot,
    &CLine3::SetSD,
};

static bool separate(float radius, float p0, float p1, EBoxTriSeperatingAxis axis, float& minDepth,
                     EBoxTriSeperatingAxis& minAxis);
static bool separate(float radius, float p0, float p1, float p2, EBoxTriSeperatingAxis axis, float& minDepth,
                     EBoxTriSeperatingAxis& minAxis);

static inline float Dot(const CVector3& a, const CVector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Raises the collision's result and returns the separation-axis verdict.
static inline int Separated(CCollision& collision, bool contacts) {
    if (contacts && collision.m_result < CLSN_NONE) {
        collision.m_result = CLSN_NONE;
    }
    return CLSN_NONE;
}

// Keeps the axis of least separation; true when `depth` separates.
static inline bool TestAxis(float depth, EBoxTriSeperatingAxis axis, float& minDepth, EBoxTriSeperatingAxis& minAxis) {
    if (ClassifyContact(depth) == CLSN_NONE) {
        return true;
    }
    if ((float)fabs(depth) < (float)fabs(minDepth)) {
        minDepth = depth;
        minAxis = axis;
    }
    return false;
}

// Squared distance from `point` to the triangle; the barycentric
// parameters go to s and t.
#define POINT_DIST(point, s_, t_)                     \
    (nearest = GetNearestPoint(point, s_, t_),        \
     (point.x - nearest.x) * (point.x - nearest.x) +  \
         (point.y - nearest.y) * (point.y - nearest.y) + (point.z - nearest.z) * (point.z - nearest.z))

// Squared distance between the capsule's axis and the triangle edge
// starting at `origin` along `dir`.
#define EDGE_DIST(origin, dir, r_, p_, d_)  \
    edge.SetSD(origin, dir);                \
    seg.GetSquaredDistanceBetweenSegments(edge, r_, p_, &d_)

#define KEEP_NEAREST(r_, s_, t_) \
    if (sqrDist0 < sqrDist) {    \
        sqrDist = sqrDist0;      \
        r = r_;                  \
        s = s_;                  \
        t = t_;                  \
    }

int CCDBTriangle::TestCollision(const CVolCapsule& capsule, CCollision& collision, const CDBTri* tri,
                                bool contacts) const {
    const CLine3& seg = capsule.m_line;
    CVector3 start = seg.m_start;
    CVector3 end = seg.m_end;
    CVector3 dir = seg.m_dir;
    CVector3 diff;
    CVector3 e0;
    CVector3 e1;
    CVector3 e2;
    CVector3 normal;
    CLine3 edge;
    CVector3 nearest;
    float r, s, t;
    float r0, s0, t0;
    float sqrDist, sqrDist0;

    diff.x = m_v0.x - start.x;
    diff.y = m_v0.y - start.y;
    diff.z = m_v0.z - start.z;
    e0.x = m_v1.x - m_v0.x;
    e0.y = m_v1.y - m_v0.y;
    e0.z = m_v1.z - m_v0.z;
    e1.x = m_v2.x - m_v0.x;
    e1.y = m_v2.y - m_v0.y;
    e1.z = m_v2.z - m_v0.z;
    e2.x = m_v2.x - m_v1.x;
    e2.y = m_v2.y - m_v1.y;
    e2.z = m_v2.z - m_v1.z;
    normal.Cross(e0, e1);

    float normalLength = normal.Length();
    if ((float)fabs(Dot(normal, dir)) > 0.001745328f * (normalLength * dir.Length())) {
        // Not parallel: solve for the closest points, then clamp by region.
        CVector3 proj;
        proj.DotDotDot(dir, e0, e1, dir);
        float a00 = proj.x;
        float a01 = -proj.y;
        float a02 = -proj.z;
        float a11 = Dot(e0, e0);
        float a12 = Dot(e0, e1);
        float a22 = Dot(e1, e1);
        float b0 = -Dot(diff, dir);
        float b1 = Dot(diff, e0);
        float b2 = Dot(diff, e1);
        float cof00 = a11 * a22 - a12 * a12;
        float cof01 = a02 * a12 - a01 * a22;
        float cof02 = a01 * a12 - a02 * a11;
        float invDet = 1.0f / (a00 * cof00 + a01 * cof01 + a02 * cof02);
        float rhs0 = -b0 * invDet;
        float rhs1 = -b1 * invDet;
        float rhs2 = -b2 * invDet;
        float cof11 = a00 * a22 - a02 * a02;
        float cof12 = a02 * a01 - a00 * a12;
        float cof22 = a00 * a11 - a01 * a01;
        r = cof00 * rhs0 + cof01 * rhs1 + cof02 * rhs2;
        s = cof01 * rhs0 + cof11 * rhs1 + cof12 * rhs2;
        t = cof02 * rhs0 + cof12 * rhs1 + cof22 * rhs2;

        if (r < 0.0f) {
            if (s + t <= 1.0f) {
                if (s < 0.0f) {
                    if (t < 0.0f) {
                        EDGE_DIST(m_v0, e1, r, t, sqrDist);
                        s = 0.0f;
                        EDGE_DIST(m_v0, e0, r0, s0, sqrDist0);
                        t0 = 0.0f;
                        KEEP_NEAREST(r0, s0, t0);
                        sqrDist0 = POINT_DIST(start, &s0, &t0);
                        r0 = 0.0f;
                        KEEP_NEAREST(0.0f, s0, t0);
                    } else {
                        EDGE_DIST(m_v0, e1, r, t, sqrDist);
                        s = 0.0f;
                        sqrDist0 = POINT_DIST(start, &s0, &t0);
                        r0 = 0.0f;
                        KEEP_NEAREST(0.0f, s0, t0);
                    }
                } else if (t < 0.0f) {
                    EDGE_DIST(m_v0, e0, r, s, sqrDist);
                    t = 0.0f;
                    sqrDist0 = POINT_DIST(start, &s0, &t0);
                    r0 = 0.0f;
                    KEEP_NEAREST(0.0f, s0, t0);
                } else {
                    sqrDist = POINT_DIST(start, &s, &t);
                    r = 0.0f;
                }
            } else if (s < 0.0f) {
                EDGE_DIST(m_v0, e1, r, t, sqrDist);
                s = 0.0f;
                EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
                s0 = 1.0f - t0;
                KEEP_NEAREST(r0, s0, t0);
                sqrDist0 = POINT_DIST(start, &s0, &t0);
                r0 = 0.0f;
                KEEP_NEAREST(0.0f, s0, t0);
            } else if (t < 0.0f) {
                EDGE_DIST(m_v0, e0, r, s, sqrDist);
                t = 0.0f;
                EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
                s0 = 1.0f - t0;
                KEEP_NEAREST(r0, s0, t0);
                sqrDist0 = POINT_DIST(start, &s0, &t0);
                r0 = 0.0f;
                KEEP_NEAREST(0.0f, s0, t0);
            } else {
                EDGE_DIST(m_v1, e2, r, t, sqrDist);
                s = 1.0f - t;
                sqrDist0 = POINT_DIST(start, &s0, &t0);
                r0 = 0.0f;
                KEEP_NEAREST(0.0f, s0, t0);
            }
        } else if (r <= 1.0f) {
            if (s + t <= 1.0f) {
                if (s < 0.0f) {
                    if (t < 0.0f) {
                        EDGE_DIST(m_v0, e1, r, t, sqrDist);
                        s = 0.0f;
                        EDGE_DIST(m_v0, e0, r0, s0, sqrDist0);
                        t0 = 0.0f;
                        KEEP_NEAREST(r0, s0, t0);
                    } else {
                        EDGE_DIST(m_v0, e1, r, t, sqrDist);
                        s = 0.0f;
                    }
                } else if (t < 0.0f) {
                    EDGE_DIST(m_v0, e0, r, s, sqrDist);
                    t = 0.0f;
                } else {
                    // Interior: the segment's closest point lies over the face.
                    sqrDist = r * (a00 * r + a01 * s + a02 * t + 2.0f * b0) +
                              s * (a01 * r + a11 * s + a12 * t + 2.0f * b1) +
                              t * (a02 * r + a12 * s + a22 * t + 2.0f * b2) + Dot(diff, diff);
                }
            } else if (s < 0.0f) {
                EDGE_DIST(m_v0, e1, r, t, sqrDist);
                s = 0.0f;
                EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
                s0 = 1.0f - t0;
                KEEP_NEAREST(r0, s0, t0);
            } else if (t < 0.0f) {
                EDGE_DIST(m_v0, e0, r, s, sqrDist);
                t = 0.0f;
                EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
                s0 = 1.0f - t0;
                KEEP_NEAREST(r0, s0, t0);
            } else {
                EDGE_DIST(m_v1, e2, r, t, sqrDist);
                s = 1.0f - t;
            }
        } else {
            if (s + t <= 1.0f) {
                if (s < 0.0f) {
                    if (t < 0.0f) {
                        EDGE_DIST(m_v0, e1, r, t, sqrDist);
                        s = 0.0f;
                        EDGE_DIST(m_v0, e0, r0, s0, sqrDist0);
                        t0 = 0.0f;
                        KEEP_NEAREST(r0, s0, t0);
                        sqrDist0 = POINT_DIST(end, &s0, &t0);
                        r0 = 1.0f;
                        KEEP_NEAREST(1.0f, s0, t0);
                    } else {
                        EDGE_DIST(m_v0, e1, r, t, sqrDist);
                        s = 0.0f;
                        sqrDist0 = POINT_DIST(end, &s0, &t0);
                        r0 = 1.0f;
                        KEEP_NEAREST(1.0f, s0, t0);
                    }
                } else if (t < 0.0f) {
                    EDGE_DIST(m_v0, e0, r, s, sqrDist);
                    t = 0.0f;
                    sqrDist0 = POINT_DIST(end, &s0, &t0);
                    r0 = 1.0f;
                    KEEP_NEAREST(1.0f, s0, t0);
                } else {
                    sqrDist = POINT_DIST(end, &s, &t);
                    r = 1.0f;
                }
            } else if (s < 0.0f) {
                EDGE_DIST(m_v0, e1, r, t, sqrDist);
                s = 0.0f;
                EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
                s0 = 1.0f - t0;
                KEEP_NEAREST(r0, s0, t0);
                sqrDist0 = POINT_DIST(end, &s0, &t0);
                r0 = 1.0f;
                KEEP_NEAREST(1.0f, s0, t0);
            } else if (t < 0.0f) {
                EDGE_DIST(m_v0, e0, r, s, sqrDist);
                t = 0.0f;
                EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
                s0 = 1.0f - t0;
                KEEP_NEAREST(r0, s0, t0);
                sqrDist0 = POINT_DIST(end, &s0, &t0);
                r0 = 1.0f;
                KEEP_NEAREST(1.0f, s0, t0);
            } else {
                EDGE_DIST(m_v1, e2, r, t, sqrDist);
                s = 1.0f - t;
                sqrDist0 = POINT_DIST(end, &s0, &t0);
                r0 = 1.0f;
                KEEP_NEAREST(1.0f, s0, t0);
            }
        }
    } else {
        // Parallel: the minimum is on an edge or at an end point.
        EDGE_DIST(m_v0, e0, r, s, sqrDist);
        t = 0.0f;
        EDGE_DIST(m_v0, e1, r0, t0, sqrDist0);
        s0 = 0.0f;
        KEEP_NEAREST(r0, 0.0f, t0);
        EDGE_DIST(m_v1, e2, r0, t0, sqrDist0);
        s0 = 1.0f - t0;
        KEEP_NEAREST(r0, s0, t0);
        sqrDist0 = POINT_DIST(start, &s0, &t0);
        r0 = 0.0f;
        KEEP_NEAREST(0.0f, s0, t0);
        sqrDist0 = POINT_DIST(end, &s0, &t0);
        r0 = 1.0f;
        KEEP_NEAREST(1.0f, s0, t0);
    }

    float depth = sqrtf(sqrDist) - capsule.m_radius;
    int result = ClassifyContact(depth);
    if (contacts) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            CVector3 triPoint;
            triPoint.x = m_v0.x + s * e0.x;
            triPoint.y = m_v0.y + s * e0.y;
            triPoint.z = m_v0.z + s * e0.z;
            triPoint.x = triPoint.x + t * e1.x;
            triPoint.y = triPoint.y + t * e1.y;
            triPoint.z = triPoint.z + t * e1.z;
            diff.x = (start.x + r * dir.x) - triPoint.x;
            diff.y = (start.y + r * dir.y) - triPoint.y;
            diff.z = (start.z + r * dir.z) - triPoint.z;
            diff.Normalize();
            SClsnContact contact;
            contact.m_depth = depth;
            contact.m_point = triPoint;
            contact.m_normal = diff;
            contact.m_pPoly = (const SClsnPoly*)tri;
            collision.AddContact(contact);
        }
    }
    return result;
}

#undef EDGE_DIST
#undef KEEP_NEAREST
#undef POINT_DIST

// Animated volumes collide as their bounding sphere.
int CCDBTriangle::TestCollision(const CAnimatedVolume& volume, CCollision& collision, const CDBTri* tri,
                                bool contacts) const {
    CVector3 nearest = GetNearestPoint(volume.GetCenter(), NULL, NULL);
    CVector3 center = volume.GetCenter();
    float dx = center.x - nearest.x;
    float dy = center.y - nearest.y;
    float dz = center.z - nearest.z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float depth = dist - volume.GetRadius();
    int result = ClassifyContact(depth);
    if (contacts) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            float inv = 1.0f / dist;
            SClsnContact contact;
            contact.m_point = nearest;
            contact.m_depth = depth;
            contact.m_pPoly = (const SClsnPoly*)tri;
            contact.m_normal.x = inv * dx;
            contact.m_normal.y = inv * dy;
            contact.m_normal.z = inv * dz;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CCDBTriangle::TestCollision(const CVolSphere& sphere, CCollision& collision, const CDBTri* tri,
                                bool contacts) const {
    CVector3 nearest = GetNearestPoint(sphere.GetCenter(), NULL, NULL);
    CVector3 center = sphere.GetCenter();
    float dx = center.x - nearest.x;
    float dy = center.y - nearest.y;
    float dz = center.z - nearest.z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float depth = dist - sphere.GetRadius();
    int result = ClassifyContact(depth);
    if (contacts) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            float inv = 1.0f / dist;
            SClsnContact contact;
            contact.m_point = nearest;
            contact.m_depth = depth;
            contact.m_pPoly = (const SClsnPoly*)tri;
            contact.m_normal.x = inv * dx;
            contact.m_normal.y = inv * dy;
            contact.m_normal.z = inv * dz;
            collision.AddContact(contact);
        }
    }
    return result;
}

CVector3 CCDBTriangle::GetNearestPoint(CVector3 point, float* sParam, float* tParam) const {
    float e0x = m_v1.x - m_v0.x;
    float e0y = m_v1.y - m_v0.y;
    float e0z = m_v1.z - m_v0.z;
    float e1x = m_v2.x - m_v0.x;
    float e1y = m_v2.y - m_v0.y;
    float e1z = m_v2.z - m_v0.z;
    float dx = m_v0.x - point.x;
    float dy = m_v0.y - point.y;
    float dz = m_v0.z - point.z;
    float a00 = e0x * e0x + e0y * e0y + e0z * e0z;
    float a01 = e1x * e0x + e1y * e0y + e1z * e0z;
    float a11 = e1x * e1x + e1y * e1y + e1z * e1z;
    float b0 = dx * e0x + dy * e0y + dz * e0z;
    float b1 = dx * e1x + dy * e1y + dz * e1z;
    float det = (float)fabs(a00 * a11 - a01 * a01);
    float s = a01 * b1 - a11 * b0;
    float t = a01 * b0 - a00 * b1;

    if (s + t <= det) {
        if (s < 0.0f) {
            if (t < 0.0f) {
                // region 4
                if (b0 < 0.0f) {
                    t = 0.0f;
                    if (-b0 >= a00) {
                        s = 1.0f;
                    } else {
                        s = -b0 / a00;
                    }
                } else {
                    s = 0.0f;
                    if (b1 >= 0.0f) {
                        t = 0.0f;
                    } else if (-b1 >= a11) {
                        t = 1.0f;
                    } else {
                        t = -b1 / a11;
                    }
                }
            } else {
                // region 3
                s = 0.0f;
                if (b1 >= 0.0f) {
                    t = 0.0f;
                } else if (-b1 >= a11) {
                    t = 1.0f;
                } else {
                    t = -b1 / a11;
                }
            }
        } else if (t < 0.0f) {
            // region 5
            t = 0.0f;
            if (b0 >= 0.0f) {
                s = 0.0f;
            } else if (-b0 >= a00) {
                s = 1.0f;
            } else {
                s = -b0 / a00;
            }
        } else {
            // region 0
            float invDet = 1.0f / det;
            s *= invDet;
            t *= invDet;
        }
    } else if (s < 0.0f) {
        // region 2
        float tmp0 = a01 + b0;
        float tmp1 = a11 + b1;
        if (tmp1 > tmp0) {
            float numer = tmp1 - tmp0;
            float denom = a00 - 2.0f * a01 + a11;
            if (numer >= denom) {
                t = 0.0f;
                s = 1.0f;
            } else {
                s = numer / denom;
                t = 1.0f - s;
            }
        } else {
            s = 0.0f;
            if (tmp1 <= 0.0f) {
                t = 1.0f;
            } else if (b1 >= 0.0f) {
                t = 0.0f;
            } else {
                t = -b1 / a11;
            }
        }
    } else if (t < 0.0f) {
        // region 6
        float tmp0 = a01 + b1;
        float tmp1 = a00 + b0;
        if (tmp1 > tmp0) {
            float numer = tmp1 - tmp0;
            float denom = a00 - 2.0f * a01 + a11;
            if (numer >= denom) {
                s = 0.0f;
                t = 1.0f;
            } else {
                t = numer / denom;
                s = 1.0f - t;
            }
        } else {
            t = 0.0f;
            if (tmp1 <= 0.0f) {
                s = 1.0f;
            } else if (b0 >= 0.0f) {
                s = 0.0f;
            } else {
                s = -b0 / a00;
            }
        }
    } else {
        // region 1
        float numer = a11 + b1 - a01 - b0;
        if (numer <= 0.0f) {
            s = 0.0f;
            t = 1.0f;
        } else {
            float denom = a00 - 2.0f * a01 + a11;
            if (numer >= denom) {
                t = 0.0f;
                s = 1.0f;
            } else {
                s = numer / denom;
                t = 1.0f - s;
            }
        }
    }

    CVector3 nearest;
    nearest.x = m_v0.x + s * e0x;
    nearest.y = m_v0.y + s * e0y;
    nearest.z = m_v0.z + s * e0z;
    nearest.x = nearest.x + t * e1x;
    nearest.y = nearest.y + t * e1y;
    nearest.z = nearest.z + t * e1z;
    if (sParam != NULL) {
        *sParam = s;
    }
    if (tParam != NULL) {
        *tParam = t;
    }
    return nearest;
}

// Separating-axis test of the triangle against an oriented box: the three
// box axes, the triangle normal and the nine edge cross products. The
// contact normal is the axis of least penetration.
int CCDBTriangle::TestCollision(const CVolBox& box, CCollision& collision, const CDBTri* tri, bool contacts) const {
    CVector3 center = box.m_center;
    float minDepth = CDB_FLT_MAX;
    EBoxTriSeperatingAxis minAxis = BOXTRI_AXIS_NONE;
    CVector3 diff;
    CVector3 e0;
    CVector3 e1;
    CVector3 proj;
    float e2x = m_v2.x - m_v1.x;
    float e2y = m_v2.y - m_v1.y;
    float e2z = m_v2.z - m_v1.z;
    float hx = box.m_halfSize[0];
    float hy;
    float hz;

    diff.x = m_v0.x - center.x;
    diff.y = m_v0.y - center.y;
    diff.z = m_v0.z - center.z;
    e0.x = m_v1.x - m_v0.x;
    e0.y = m_v1.y - m_v0.y;
    e0.z = m_v1.z - m_v0.z;
    e1.x = m_v2.x - m_v0.x;
    e1.y = m_v2.y - m_v0.y;
    e1.z = m_v2.z - m_v0.z;

    CVector3 axisX = box.m_axisX;
    proj.DotDotDot(diff, e0, e1, axisX);
    if (separate(hx, proj.x, proj.y, proj.z, BOXTRI_AXIS_BOX_X, minDepth, minAxis)) {
        return Separated(collision, contacts);
    }
    CVector3 axisY = box.m_axisY;
    hy = box.m_halfSize[1];
    proj.DotDotDot(diff, e0, e1, axisY);
    if (separate(hy, proj.x, proj.y, proj.z, BOXTRI_AXIS_BOX_Y, minDepth, minAxis)) {
        return Separated(collision, contacts);
    }
    CVector3 axisZ = box.m_axisZ;
    hz = box.m_halfSize[2];
    proj.DotDotDot(diff, e0, e1, axisZ);
    if (separate(hz, proj.x, proj.y, proj.z, BOXTRI_AXIS_BOX_Z, minDepth, minAxis)) {
        return Separated(collision, contacts);
    }

    CVector3 normal;
    normal.Cross(e0, e1);
    normal.Normalize();
    proj.DotDotDot(axisX, axisY, axisZ, normal);
    proj.x = (float)fabs(proj.x);
    proj.y = (float)fabs(proj.y);
    proj.z = (float)fabs(proj.z);
    if (TestAxis((float)fabs(Dot(normal, diff)) - (hx * proj.x + hy * proj.y + hz * proj.z), BOXTRI_AXIS_NORMAL,
                 minDepth, minAxis)) {
        return Separated(collision, contacts);
    }

    // Box axis x edges; edge 2 (v1 -> v2) needs all three projections.
    CVector3 axisXe0;
    axisXe0.Cross(axisX, e0);
    if (axisXe0.Length() > 0.001f) {
        axisXe0.Normalize();
        if (separate(hy * (float)fabs(Dot(axisY, axisXe0)) + hz * (float)fabs(Dot(axisZ, axisXe0)),
                     Dot(axisXe0, diff), Dot(axisXe0, e1), BOXTRI_AXIS_X_EDGE0, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }
    CVector3 axisXe1;
    axisXe1.Cross(axisX, e1);
    if (axisXe1.Normalize() > 0.001f) {
        if (separate(hy * (float)fabs(Dot(axisY, axisXe1)) + hz * (float)fabs(Dot(axisZ, axisXe1)),
                     Dot(axisXe1, diff), Dot(axisXe1, e0), BOXTRI_AXIS_X_EDGE1, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }
    CVector3 axisXe2;
    axisXe2.y = axisX.z * e2x - axisX.x * e2z;
    axisXe2.x = axisX.y * e2z - axisX.z * e2y;
    axisXe2.z = axisX.x * e2y - axisX.y * e2x;
    if (axisXe2.Normalize() > 0.001f) {
        proj.DotDotDot(diff, e0, e1, axisXe2);
        if (separate(hy * (float)fabs(Dot(axisY, axisXe2)) + hz * (float)fabs(Dot(axisZ, axisXe2)), proj.x, proj.y,
                     proj.z, BOXTRI_AXIS_X_EDGE2, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }

    CVector3 axisYe0;
    axisYe0.Cross(axisY, e0);
    if (axisYe0.Normalize() > 0.001f) {
        if (separate(hx * (float)fabs(Dot(axisX, axisYe0)) + hz * (float)fabs(Dot(axisZ, axisYe0)),
                     Dot(axisYe0, diff), Dot(axisYe0, e1), BOXTRI_AXIS_Y_EDGE0, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }
    CVector3 axisYe1;
    axisYe1.Cross(axisY, e1);
    if (axisYe1.Normalize() > 0.001f) {
        if (separate(hx * (float)fabs(Dot(axisX, axisYe1)) + hz * (float)fabs(Dot(axisZ, axisYe1)),
                     Dot(axisYe1, diff), Dot(axisYe1, e0), BOXTRI_AXIS_Y_EDGE1, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }
    CVector3 axisYe2;
    axisYe2.y = axisY.z * e2x - axisY.x * e2z;
    axisYe2.x = axisY.y * e2z - axisY.z * e2y;
    axisYe2.z = axisY.x * e2y - axisY.y * e2x;
    if (axisYe2.Normalize() > 0.001f) {
        proj.DotDotDot(diff, e0, e1, axisYe2);
        if (separate(hx * (float)fabs(Dot(axisX, axisYe2)) + hz * (float)fabs(Dot(axisZ, axisYe2)), proj.x, proj.y,
                     proj.z, BOXTRI_AXIS_Y_EDGE2, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }

    CVector3 axisZe0;
    axisZe0.Cross(axisZ, e0);
    if (axisZe0.Normalize() > 0.001f) {
        if (separate(hx * (float)fabs(Dot(axisX, axisZe0)) + hy * (float)fabs(Dot(axisY, axisZe0)),
                     Dot(axisZe0, diff), Dot(axisZe0, e1), BOXTRI_AXIS_Z_EDGE0, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }
    CVector3 axisZe1;
    axisZe1.Cross(axisZ, e1);
    if (axisZe1.Normalize() > 0.001f) {
        if (separate(hx * (float)fabs(Dot(axisX, axisZe1)) + hy * (float)fabs(Dot(axisY, axisZe1)),
                     Dot(axisZe1, diff), Dot(axisZe1, e0), BOXTRI_AXIS_Z_EDGE1, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }
    CVector3 axisZe2;
    axisZe2.y = axisZ.z * e2x - axisZ.x * e2z;
    axisZe2.x = axisZ.y * e2z - axisZ.z * e2y;
    axisZe2.z = axisZ.x * e2y - axisZ.y * e2x;
    if (axisZe2.Normalize() > 0.001f) {
        proj.DotDotDot(diff, e0, e1, axisZe2);
        if (separate(hx * (float)fabs(Dot(axisX, axisZe2)) + hy * (float)fabs(Dot(axisY, axisZe2)), proj.x, proj.y,
                     proj.z, BOXTRI_AXIS_Z_EDGE2, minDepth, minAxis)) {
            return Separated(collision, contacts);
        }
    }

    int result = ClassifyContact(minDepth);
    if (contacts) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            // The contact point is left unset, as in the original.
            SClsnContact contact;
            contact.m_depth = minDepth;
            switch (minAxis) {
            case BOXTRI_AXIS_BOX_X:
                contact.m_normal = axisX;
                break;
            case BOXTRI_AXIS_BOX_Y:
                contact.m_normal = axisY;
                break;
            case BOXTRI_AXIS_BOX_Z:
                contact.m_normal = axisZ;
                break;
            case BOXTRI_AXIS_NORMAL:
                contact.m_normal = normal;
                break;
            case BOXTRI_AXIS_X_EDGE0:
                contact.m_normal = axisXe0;
                break;
            case BOXTRI_AXIS_X_EDGE1:
                contact.m_normal = axisXe1;
                break;
            case BOXTRI_AXIS_X_EDGE2:
                contact.m_normal = axisXe2;
                break;
            case BOXTRI_AXIS_Y_EDGE0:
                contact.m_normal = axisYe0;
                break;
            case BOXTRI_AXIS_Y_EDGE1:
                contact.m_normal = axisYe1;
                break;
            case BOXTRI_AXIS_Y_EDGE2:
                contact.m_normal = axisYe2;
                break;
            case BOXTRI_AXIS_Z_EDGE0:
                contact.m_normal = axisZe0;
                break;
            case BOXTRI_AXIS_Z_EDGE1:
                contact.m_normal = axisZe1;
                break;
            case BOXTRI_AXIS_Z_EDGE2:
                contact.m_normal = axisZe2;
                break;
            }
            contact.m_pPoly = (const SClsnPoly*)tri;
            collision.AddContact(contact);
        }
    }
    return result;
}

// Line segment against the triangle (Moller-Trumbore). Triangles flagged
// m_lineSkip are ignored unless the collision asks for every polygon.
int CCDBTriangle::TestCollision(const CLine3& line, CCollision& collision, const CDBTri* tri, bool contacts) const {
    if (!(collision.m_flags & 0x80) && tri->m_lineSkip) {
        return CLSN_NONE;
    }
    CVector3 dir = line.m_dir;
    float e1x = m_v2.x - m_v0.x;
    float e1y = m_v2.y - m_v0.y;
    float e1z = m_v2.z - m_v0.z;
    float e0x = m_v1.x - m_v0.x;
    float e0y = m_v1.y - m_v0.y;
    float e0z = m_v1.z - m_v0.z;
    float px = dir.y * e1z - dir.z * e1y;
    float py = dir.z * e1x - dir.x * e1z;
    float pz = dir.x * e1y - dir.y * e1x;
    float det = e0x * px + e0y * py + e0z * pz;
    if ((float)fabs(det) < 0.00001f) {
        return Separated(collision, contacts);
    }
    float invDet = 1.0f / det;
    CVector3 start = line.m_start;
    float tx = start.x - m_v0.x;
    float ty = start.y - m_v0.y;
    float tz = start.z - m_v0.z;
    float u = invDet * (tx * px + ty * py + tz * pz);
    if (u < 0.0f || u > 1.0f) {
        return Separated(collision, contacts);
    }
    float qx = ty * e0z - tz * e0y;
    float qy = tz * e0x - tx * e0z;
    float qz = tx * e0y - ty * e0x;
    float v = invDet * (dir.x * qx + dir.y * qy + dir.z * qz);
    if (v < 0.0f || u + v > 1.0f) {
        return Separated(collision, contacts);
    }
    float t = invDet * (e1x * qx + e1y * qy + e1z * qz);
    if (t < 0.0f || t > 1.0f) {
        return Separated(collision, contacts);
    }

    if (contacts) {
        collision.ClearContacts();
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
    CVector3 lineEnd = line.m_end;
    dir.Normalize();
    float depth = dir.x * (hit.x - lineEnd.x) + dir.y * (hit.y - lineEnd.y) + dir.z * (hit.z - lineEnd.z);
    int result = ClassifyContact(depth);
    if (contacts) {
        if (collision.m_flags & 0x80) {
            result = CLSN_TOUCHING;
            depth = 0.0f;
        }
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            CVector3 normal;
            normal.x = e0y * e1z - e0z * e1y;
            normal.y = e0z * e1x - e0x * e1z;
            normal.z = e0x * e1y - e0y * e1x;
            normal.Normalize();
            SClsnContact contact;
            contact.m_point = hit;
            contact.m_normal = normal;
            contact.m_depth = depth;
            contact.m_pPoly = (const SClsnPoly*)tri;
            collision.AddContact(contact);
        }
    }
    return result;
}

CCDBTriangle::CCDBTriangle(const CDBTri& tri) {
    const CDBVector* v = tri.m_verts[0];
    m_v0.x = v->x;
    m_v0.y = v->y;
    m_v0.z = v->z;
    v = tri.m_verts[1];
    m_v1.x = v->x;
    m_v1.y = v->y;
    m_v1.z = v->z;
    v = tri.m_verts[2];
    m_v2.x = v->x;
    m_v2.y = v->y;
    m_v2.z = v->z;
}

// Box against a triangle: separating axes only (no contacts, and the
// collision's result is left alone). The edge axes are not normalized and
// use the unit normal's projections, as in the original.
int CCDBVolBox::TestCollision(const CTriangle& tri, CCollision&, bool) const {
    float minDepth = CDB_FLT_MAX;
    EBoxTriSeperatingAxis minAxis = BOXTRI_AXIS_NONE;
    float dx = tri.m_v0.x - m_center.x;
    float dy = tri.m_v0.y - m_center.y;
    float dz = tri.m_v0.z - m_center.z;
    float e0x = tri.m_v1.x - tri.m_v0.x;
    float e0y = tri.m_v1.y - tri.m_v0.y;
    float e0z = tri.m_v1.z - tri.m_v0.z;
    float e1x = tri.m_v2.x - tri.m_v0.x;
    float e1y = tri.m_v2.y - tri.m_v0.y;
    float e1z = tri.m_v2.z - tri.m_v0.z;
    const CVector3& X = m_axisX;
    const CVector3& Y = m_axisY;
    const CVector3& Z = m_axisZ;
    float hx = m_halfSize[0];
    float hy = m_halfSize[1];
    float hz = m_halfSize[2];

    float xe0 = e0x * X.x + e0y * X.y + e0z * X.z;
    float xe1 = e1x * X.x + e1y * X.y + e1z * X.z;
    if (separate(hx, dx * X.x + dy * X.y + dz * X.z, xe0, xe1, BOXTRI_AXIS_BOX_X, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float ye0 = e0x * Y.x + e0y * Y.y + e0z * Y.z;
    float ye1 = e1x * Y.x + e1y * Y.y + e1z * Y.z;
    if (separate(hy, dx * Y.x + dy * Y.y + dz * Y.z, ye0, ye1, BOXTRI_AXIS_BOX_Y, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float ze0 = e0x * Z.x + e0y * Z.y + e0z * Z.z;
    float ze1 = e1x * Z.x + e1y * Z.y + e1z * Z.z;
    if (separate(hz, dx * Z.x + dy * Z.y + dz * Z.z, ze0, ze1, BOXTRI_AXIS_BOX_Z, minDepth, minAxis)) {
        return CLSN_NONE;
    }

    CVector3 n;
    n.x = e0y * e1z - e0z * e1y;
    n.y = e0z * e1x - e0x * e1z;
    n.z = e0x * e1y - e0y * e1x;
    n.Normalize();
    float xn = X.x * n.x + X.y * n.y + X.z * n.z;
    float yn = Y.x * n.x + Y.y * n.y + Y.z * n.z;
    float zn = Z.x * n.x + Z.y * n.y + Z.z * n.z;
    if (TestAxis((float)fabs(n.x * dx + n.y * dy + n.z * dz) -
                     ((float)fabs(xn) * hx + (float)fabs(yn) * hy + (float)fabs(zn) * hz),
                 BOXTRI_AXIS_NORMAL, minDepth, minAxis)) {
        return CLSN_NONE;
    }

    // D.(A x e) for box axis A and triangle edge e.
#define TRIPLE(A, ex, ey, ez) \
    (((A).y * (ez) - (A).z * (ey)) * dx + ((A).z * (ex) - (A).x * (ez)) * dy + ((A).x * (ey) - (A).y * (ex)) * dz)

    float pxe0 = TRIPLE(X, e0x, e0y, e0z);
    if (separate(hy * (float)fabs(ze0) + hz * (float)fabs(ye0), pxe0, xn, BOXTRI_AXIS_X_EDGE0, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float pxe1 = TRIPLE(X, e1x, e1y, e1z);
    if (separate(hy * (float)fabs(ze1) + hz * (float)fabs(ye1), pxe1, -xn, BOXTRI_AXIS_X_EDGE1, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    if (separate(hy * (float)fabs(ze1 - ze0) + hz * (float)fabs(ye1 - ye0), pxe1 - pxe0, -xn, BOXTRI_AXIS_X_EDGE2,
                 minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float pye0 = TRIPLE(Y, e0x, e0y, e0z);
    if (separate(hx * (float)fabs(ze0) + hz * (float)fabs(xe0), pye0, yn, BOXTRI_AXIS_Y_EDGE0, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float pye1 = TRIPLE(Y, e1x, e1y, e1z);
    if (separate(hx * (float)fabs(ze1) + hz * (float)fabs(xe1), pye1, -yn, BOXTRI_AXIS_Y_EDGE1, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    if (separate(hx * (float)fabs(ze1 - ze0) + hz * (float)fabs(xe1 - xe0), pye1 - pye0, -yn, BOXTRI_AXIS_Y_EDGE2,
                 minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float pze0 = TRIPLE(Z, e0x, e0y, e0z);
    if (separate(hx * (float)fabs(ye0) + hy * (float)fabs(xe0), pze0, zn, BOXTRI_AXIS_Z_EDGE0, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    float pze1 = TRIPLE(Z, e1x, e1y, e1z);
    if (separate(hx * (float)fabs(ye1) + hy * (float)fabs(xe1), pze1, -zn, BOXTRI_AXIS_Z_EDGE1, minDepth, minAxis)) {
        return CLSN_NONE;
    }
    if (separate(hx * (float)fabs(ye1 - ye0) + hy * (float)fabs(xe1 - xe0), pze1 - pze0, -zn, BOXTRI_AXIS_Z_EDGE2,
                 minDepth, minAxis)) {
        return CLSN_NONE;
    }
#undef TRIPLE

    return ClassifyContact(minDepth);
}

// Clips the line parameter range [tMin, tMax] to the box's slab along
// `axis`; false when the line misses it.
static inline bool ClipSlab(const CVector3& axis, const CVector3& dir, const CVector3& diff, float halfSize,
                            float& tMin, float& tMax) {
    float denom = axis.x * dir.x + axis.y * dir.y + axis.z * dir.z;
    float dist = axis.x * diff.x + axis.y * diff.y + axis.z * diff.z;
    if ((float)fabs(denom) > 0.00001f) {
        float inv = 1.0f / denom;
        float t1 = inv * (dist + halfSize);
        float t2 = inv * (dist - halfSize);
        if (t1 > t2) {
            if (t2 > tMin) {
                tMin = t2;
            }
            if (t1 < tMax) {
                tMax = t1;
            }
        } else {
            if (t1 > tMin) {
                tMin = t1;
            }
            if (t2 < tMax) {
                tMax = t2;
            }
        }
        if (tMin > tMax) {
            return false;
        }
        if (tMax < 0.0f) {
            return false;
        }
        return true;
    } else if (dist > halfSize || dist < -halfSize) {
        return false;
    }
    return true;
}

// Line segment against the box (slab test): CLSN_PENETRATING on a hit, no
// contacts.
int CCDBVolBox::TestCollision(const CLine3& line, CCollision&, bool) const {
    CVector3 start = line.m_start;
    CVector3 dir = line.m_dir;
    CVector3 diff;
    diff.x = m_center.x - start.x;
    diff.y = m_center.y - start.y;
    diff.z = m_center.z - start.z;
    float tMin = -CDB_FLT_MAX;
    float tMax = CDB_FLT_MAX;
    if (!ClipSlab(m_axisX, dir, diff, m_halfSize[0], tMin, tMax)) {
        return CLSN_NONE;
    }
    if (!ClipSlab(m_axisY, dir, diff, m_halfSize[1], tMin, tMax)) {
        return CLSN_NONE;
    }
    if (!ClipSlab(m_axisZ, dir, diff, m_halfSize[2], tMin, tMax)) {
        return CLSN_NONE;
    }
    if (tMin > 1.0f || tMax < 0.0f) {
        return CLSN_NONE;
    }
    return CLSN_PENETRATING;
}

int CCDBVolBox::TestCollision(const CVolFiber& fiber, CCollision& collision, bool contacts) const {
    collision.m_flags |= 0x80;
    return TestCollision(fiber.GetLine(), collision, contacts);
}

int CCDBVolBox::TestCollision(const CPlane& plane, CCollision&, bool) const {
    float nx = plane.m_normal[0];
    float ny = plane.m_normal[1];
    float nz = plane.m_normal[2];
    CVector3 center = m_center;
    float radius = (float)fabs(m_axisX.x * nx + m_axisX.y * ny + m_axisX.z * nz) * m_halfSize[0] +
                   (float)fabs(m_axisY.x * nx + m_axisY.y * ny + m_axisY.z * nz) * m_halfSize[1] +
                   (float)fabs(m_axisZ.x * nx + m_axisZ.y * ny + m_axisZ.z * nz) * m_halfSize[2];
    return ClassifyContact(((nx * center.x + ny * center.y + nz * center.z) - plane.m_d) - radius);
}

int CCDBVolBox::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool contacts) const {
    return CVolBox::TestCollision(capsule, collision, contacts);
}

int CCDBVolBox::TestCollision(const CCSGVolume&, CCollision&, bool) const {
    return 0;
}

int CCDBVolBox::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool contacts) const {
    return TestCollision((const CVolSphere&)volume, collision, contacts);
}

int CCDBVolBox::TestCollision(const CVolSphere& sphere, CCollision&, bool) const {
    CVector3 center = sphere.GetCenter();
    float dx = center.x - m_center.x;
    float dy = center.y - m_center.y;
    float dz = center.z - m_center.z;
    float sqrDist = 0.0f;
    float px = (float)fabs(m_axisX.x * dx + m_axisX.y * dy + m_axisX.z * dz);
    float pz = (float)fabs(m_axisZ.x * dx + m_axisZ.y * dy + m_axisZ.z * dz);
    float py = (float)fabs(m_axisY.x * dx + m_axisY.y * dy + m_axisY.z * dz);
    if (px > m_halfSize[0]) {
        float d = px - m_halfSize[0];
        sqrDist = d * d + sqrDist;
    }
    if (py > m_halfSize[1]) {
        float d = py - m_halfSize[1];
        sqrDist += d * d;
    }
    if (pz > m_halfSize[2]) {
        float d = pz - m_halfSize[2];
        sqrDist += d * d;
    }
    return ClassifyContact(sqrtf(sqrDist) - sphere.GetRadius());
}

CCDBVolBox::CCDBVolBox(const CDBBox& box) {
    SetBasis(CVector3(box.m_basis[0][0], box.m_basis[0][1], box.m_basis[0][2]),
             CVector3(box.m_basis[1][0], box.m_basis[1][1], box.m_basis[1][2]),
             CVector3(box.m_basis[2][0], box.m_basis[2][1], box.m_basis[2][2]));
    SetCenter(CVector3(box.m_center[0], box.m_center[1], box.m_center[2]));
    SetWidth(box.m_width);
    SetDepth(box.m_depth);
    SetHeight(box.m_height);
}

// Separation along an axis where two vertices project alike: v0 at p0 and
// the third vertex at p0 + p1. True when `radius` cannot reach.
static bool separate(float radius, float p0, float p1, EBoxTriSeperatingAxis axis, float& minDepth,
                     EBoxTriSeperatingAxis& minAxis) {
    float depth;
    if (p0 >= 0.0f) {
        if (p1 > 0.0f) {
            p1 = 0.0f;
        }
        depth = (p0 + p1) - radius;
    } else {
        if (p1 < 0.0f) {
            p1 = 0.0f;
        }
        depth = -(p0 + p1) - radius;
    }
    return TestAxis(depth, axis, minDepth, minAxis);
}

// Separation along an axis: v0 at p0, the others at p0 + p1 and p0 + p2.
static bool separate(float radius, float p0, float p1, float p2, EBoxTriSeperatingAxis axis, float& minDepth,
                     EBoxTriSeperatingAxis& minAxis) {
    float depth;
    if (p0 >= 0.0f) {
        float lo = p1 < p2 ? p1 : p2;
        if (lo > 0.0f) {
            depth = p0 - radius;
        } else {
            depth = (p0 + lo) - radius;
        }
    } else {
        float hi = p1 < p2 ? p2 : p1;
        if (hi < 0.0f) {
            depth = -p0 - radius;
        } else {
            depth = -(p0 + hi) - radius;
        }
    }
    return TestAxis(depth, axis, minDepth, minAxis);
}
