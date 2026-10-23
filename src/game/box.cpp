#include "game/box.h"

#include <math.h>
#include "dolphin/os.h"
#include "game/SClsnContact.h"
#include "game/camera.h"
#include "game/capsule.h"
#include "game/collision.h"
#include "game/draw_context.h"
#include "game/fiber.h"
#include "game/line.h"
#include "game/matrix.h"
#include "game/plane.h"
#include "game/sphere.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

// Direction components at or below this count as zero.
#define DIR_EPSILON 1.880791e-37f
#define SLAB_EPSILON 0.000001f
#define CAPSULE_AXIS_EPSILON 0.00001f
#define EDGE_EPSILON 0.00001f
#define BIG_FLOAT 3.4028235e38f

// The separating axes of the box-box test.
enum EAxis {
    AXIS_NONE,
    AXIS_A0, AXIS_A1, AXIS_A2,  // this box's faces
    AXIS_B0, AXIS_B1, AXIS_B2,  // the other box's faces
    AXIS_A0B0, AXIS_A0B1, AXIS_A0B2,  // edge pairs
    AXIS_A1B0, AXIS_A1B1, AXIS_A1B2,
    AXIS_A2B0, AXIS_A2B1, AXIS_A2B2,
};

static inline float& Comp(CVector3& v, int i) {
    return ((float*)&v)[i];
}

static inline float Comp(const CVector3& v, int i) {
    return ((const float*)&v)[i];
}

static inline float Extent(const CVolBox& box, int i) {
    return box.m_halfSize[i];
}

static inline int Miss(CCollision& collision, bool record) {
    if (record && collision.m_result < CLSN_NONE) {
        collision.m_result = CLSN_NONE;
    }
    return CLSN_NONE;
}

// The box's world-to-box transform.
static inline void WorldToBox(const CVolBox& box, CMatrix& m) {
    m.Ident();
    m.SetRight(box.m_axisX);
    m.SetFront(box.m_axisY);
    m.SetUp(box.m_axisZ);
    m.SetPos(box.m_center);
    CMatrix::s_TempMat = m;
    m.FastInverse(CMatrix::s_TempMat);
}

// Clamps a box-space point into the box, accumulating the squared distance.
static inline void ClampAxis(CVector3& pnt, int i, const CVolBox& box, float& sqrDist) {
    float e = Extent(box, i);
    if (Comp(pnt, i) < -e) {
        float delta = Comp(pnt, i) + e;
        sqrDist += delta * delta;
        Comp(pnt, i) = -e;
    } else if (Comp(pnt, i) > e) {
        float delta = Comp(pnt, i) - e;
        sqrDist += delta * delta;
        Comp(pnt, i) = e;
    }
}

// Squared distance from the line to a box face pair, for a direction with
// no zero components (see Magic Software's MgcDist3DLinBox).
static void Face(int i0, int i1, int i2, CVector3& pnt, const CVector3& dir, const CVolBox& box,
                 const CVector3& pmE, float* lineParam, float& sqrDist) {
    CVector3 ppE;
    float lSqr, inv, tmp, param, t, delta;

    Comp(ppE, i1) = Comp(pnt, i1) + Extent(box, i1);
    Comp(ppE, i2) = Comp(pnt, i2) + Extent(box, i2);
    if (Comp(dir, i0) * Comp(ppE, i1) >= Comp(dir, i1) * Comp(pmE, i0)) {
        if (Comp(dir, i0) * Comp(ppE, i2) >= Comp(dir, i2) * Comp(pmE, i0)) {
            // v[i1] >= -e[i1], v[i2] >= -e[i2] (distance = 0)
            if (lineParam != NULL) {
                Comp(pnt, i0) = Extent(box, i0);
                inv = 1.0f / Comp(dir, i0);
                Comp(pnt, i1) -= Comp(dir, i1) * Comp(pmE, i0) * inv;
                Comp(pnt, i2) -= Comp(dir, i2) * Comp(pmE, i0) * inv;
                *lineParam = -Comp(pmE, i0) * inv;
            }
        } else {
            // v[i1] >= -e[i1], v[i2] < -e[i2]
            lSqr = Comp(dir, i0) * Comp(dir, i0) + Comp(dir, i2) * Comp(dir, i2);
            tmp = lSqr * Comp(ppE, i1) -
                  Comp(dir, i1) * (Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i2) * Comp(ppE, i2));
            if (tmp <= 2.0f * lSqr * Extent(box, i1)) {
                t = tmp / lSqr;
                lSqr += Comp(dir, i1) * Comp(dir, i1);
                tmp = Comp(ppE, i1) - t;
                delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * tmp +
                        Comp(dir, i2) * Comp(ppE, i2);
                param = -delta / lSqr;
                sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + tmp * tmp +
                           Comp(ppE, i2) * Comp(ppE, i2) + delta * param;
                if (lineParam != NULL) {
                    *lineParam = param;
                    Comp(pnt, i0) = Extent(box, i0);
                    Comp(pnt, i1) = t - Extent(box, i1);
                    Comp(pnt, i2) = -Extent(box, i2);
                }
            } else {
                lSqr += Comp(dir, i1) * Comp(dir, i1);
                delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(pmE, i1) +
                        Comp(dir, i2) * Comp(ppE, i2);
                param = -delta / lSqr;
                sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(pmE, i1) * Comp(pmE, i1) +
                           Comp(ppE, i2) * Comp(ppE, i2) + delta * param;
                if (lineParam != NULL) {
                    *lineParam = param;
                    Comp(pnt, i0) = Extent(box, i0);
                    Comp(pnt, i1) = Extent(box, i1);
                    Comp(pnt, i2) = -Extent(box, i2);
                }
            }
        }
    } else {
        if (Comp(dir, i0) * Comp(ppE, i2) >= Comp(dir, i2) * Comp(pmE, i0)) {
            // v[i1] < -e[i1], v[i2] >= -e[i2]
            lSqr = Comp(dir, i0) * Comp(dir, i0) + Comp(dir, i1) * Comp(dir, i1);
            tmp = lSqr * Comp(ppE, i2) -
                  Comp(dir, i2) * (Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1));
            if (tmp <= 2.0f * lSqr * Extent(box, i2)) {
                t = tmp / lSqr;
                lSqr += Comp(dir, i2) * Comp(dir, i2);
                tmp = Comp(ppE, i2) - t;
                delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1) +
                        Comp(dir, i2) * tmp;
                param = -delta / lSqr;
                sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(ppE, i1) * Comp(ppE, i1) + tmp * tmp +
                           delta * param;
                if (lineParam != NULL) {
                    *lineParam = param;
                    Comp(pnt, i0) = Extent(box, i0);
                    Comp(pnt, i1) = -Extent(box, i1);
                    Comp(pnt, i2) = t - Extent(box, i2);
                }
            } else {
                lSqr += Comp(dir, i2) * Comp(dir, i2);
                delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1) +
                        Comp(dir, i2) * Comp(pmE, i2);
                param = -delta / lSqr;
                sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(ppE, i1) * Comp(ppE, i1) +
                           Comp(pmE, i2) * Comp(pmE, i2) + delta * param;
                if (lineParam != NULL) {
                    *lineParam = param;
                    Comp(pnt, i0) = Extent(box, i0);
                    Comp(pnt, i1) = -Extent(box, i1);
                    Comp(pnt, i2) = Extent(box, i2);
                }
            }
        } else {
            // v[i1] < -e[i1], v[i2] < -e[i2]
            lSqr = Comp(dir, i0) * Comp(dir, i0) + Comp(dir, i2) * Comp(dir, i2);
            tmp = lSqr * Comp(ppE, i1) -
                  Comp(dir, i1) * (Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i2) * Comp(ppE, i2));
            if (tmp >= 0.0f) {
                // v[i1]-edge is closest
                if (tmp <= 2.0f * lSqr * Extent(box, i1)) {
                    t = tmp / lSqr;
                    lSqr += Comp(dir, i1) * Comp(dir, i1);
                    tmp = Comp(ppE, i1) - t;
                    delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * tmp +
                            Comp(dir, i2) * Comp(ppE, i2);
                    param = -delta / lSqr;
                    sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + tmp * tmp +
                               Comp(ppE, i2) * Comp(ppE, i2) + delta * param;
                    if (lineParam != NULL) {
                        *lineParam = param;
                        Comp(pnt, i0) = Extent(box, i0);
                        Comp(pnt, i1) = t - Extent(box, i1);
                        Comp(pnt, i2) = -Extent(box, i2);
                    }
                } else {
                    lSqr += Comp(dir, i1) * Comp(dir, i1);
                    delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(pmE, i1) +
                            Comp(dir, i2) * Comp(ppE, i2);
                    param = -delta / lSqr;
                    sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(pmE, i1) * Comp(pmE, i1) +
                               Comp(ppE, i2) * Comp(ppE, i2) + delta * param;
                    if (lineParam != NULL) {
                        *lineParam = param;
                        Comp(pnt, i0) = Extent(box, i0);
                        Comp(pnt, i1) = Extent(box, i1);
                        Comp(pnt, i2) = -Extent(box, i2);
                    }
                }
                return;
            }

            lSqr = Comp(dir, i0) * Comp(dir, i0) + Comp(dir, i1) * Comp(dir, i1);
            tmp = lSqr * Comp(ppE, i2) -
                  Comp(dir, i2) * (Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1));
            if (tmp >= 0.0f) {
                // v[i2]-edge is closest
                if (tmp <= 2.0f * lSqr * Extent(box, i2)) {
                    t = tmp / lSqr;
                    lSqr += Comp(dir, i2) * Comp(dir, i2);
                    tmp = Comp(ppE, i2) - t;
                    delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1) +
                            Comp(dir, i2) * tmp;
                    param = -delta / lSqr;
                    sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(ppE, i1) * Comp(ppE, i1) +
                               tmp * tmp + delta * param;
                    if (lineParam != NULL) {
                        *lineParam = param;
                        Comp(pnt, i0) = Extent(box, i0);
                        Comp(pnt, i1) = -Extent(box, i1);
                        Comp(pnt, i2) = t - Extent(box, i2);
                    }
                } else {
                    lSqr += Comp(dir, i2) * Comp(dir, i2);
                    delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1) +
                            Comp(dir, i2) * Comp(pmE, i2);
                    param = -delta / lSqr;
                    sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(ppE, i1) * Comp(ppE, i1) +
                               Comp(pmE, i2) * Comp(pmE, i2) + delta * param;
                    if (lineParam != NULL) {
                        *lineParam = param;
                        Comp(pnt, i0) = Extent(box, i0);
                        Comp(pnt, i1) = -Extent(box, i1);
                        Comp(pnt, i2) = Extent(box, i2);
                    }
                }
                return;
            }

            // (v[i1],v[i2])-corner is closest
            lSqr += Comp(dir, i2) * Comp(dir, i2);
            delta = Comp(dir, i0) * Comp(pmE, i0) + Comp(dir, i1) * Comp(ppE, i1) +
                    Comp(dir, i2) * Comp(ppE, i2);
            param = -delta / lSqr;
            sqrDist += Comp(pmE, i0) * Comp(pmE, i0) + Comp(ppE, i1) * Comp(ppE, i1) +
                       Comp(ppE, i2) * Comp(ppE, i2) + delta * param;
            if (lineParam != NULL) {
                *lineParam = param;
                Comp(pnt, i0) = Extent(box, i0);
                Comp(pnt, i1) = -Extent(box, i1);
                Comp(pnt, i2) = -Extent(box, i2);
            }
        }
    }
}

// Direction with one zero component (i2).
static void Case0(int i0, int i1, int i2, CVector3& pnt, const CVector3& dir, const CVolBox& box,
                  float* lineParam, float& sqrDist) {
    float pmE0 = Comp(pnt, i0) - Extent(box, i0);
    float pmE1 = Comp(pnt, i1) - Extent(box, i1);
    float prod0 = pmE0 * Comp(dir, i1);
    float prod1 = pmE1 * Comp(dir, i0);
    float delta, invLSqr, inv;

    if (prod0 >= prod1) {
        // line intersects P[i0] = e[i0]
        Comp(pnt, i0) = Extent(box, i0);
        float ppE1 = Extent(box, i1) + Comp(pnt, i1);
        delta = prod0 - ppE1 * Comp(dir, i0);
        if (delta >= 0.0f) {
            invLSqr = 1.0f / (Comp(dir, i0) * Comp(dir, i0) + Comp(dir, i1) * Comp(dir, i1));
            sqrDist += invLSqr * (delta * delta);
            if (lineParam != NULL) {
                Comp(pnt, i1) = -Extent(box, i1);
                *lineParam = invLSqr * -(pmE0 * Comp(dir, i0) + ppE1 * Comp(dir, i1));
            }
        } else if (lineParam != NULL) {
            inv = 1.0f / Comp(dir, i0);
            Comp(pnt, i1) = Comp(pnt, i1) - prod0 * inv;
            *lineParam = -pmE0 * inv;
        }
    } else {
        // line intersects P[i1] = e[i1]
        Comp(pnt, i1) = Extent(box, i1);
        float ppE0 = Extent(box, i0) + Comp(pnt, i0);
        delta = prod1 - ppE0 * Comp(dir, i1);
        if (delta >= 0.0f) {
            invLSqr = 1.0f / (Comp(dir, i1) * Comp(dir, i1) + Comp(dir, i0) * Comp(dir, i0));
            sqrDist += invLSqr * (delta * delta);
            if (lineParam != NULL) {
                Comp(pnt, i0) = -Extent(box, i0);
                *lineParam = invLSqr * -(ppE0 * Comp(dir, i0) + pmE1 * Comp(dir, i1));
            }
        } else if (lineParam != NULL) {
            inv = 1.0f / Comp(dir, i1);
            Comp(pnt, i0) = Comp(pnt, i0) - prod1 * inv;
            *lineParam = -pmE1 * inv;
        }
    }
    ClampAxis(pnt, i2, box, sqrDist);
}

// Direction with two zero components (i1, i2).
static void Case00(int i0, int i1, int i2, CVector3& pnt, const CVector3& dir, const CVolBox& box,
                   float* lineParam, float& sqrDist) {
    if (lineParam != NULL) {
        *lineParam = (Extent(box, i0) - Comp(pnt, i0)) / Comp(dir, i0);
    }
    Comp(pnt, i0) = Extent(box, i0);
    ClampAxis(pnt, i1, box, sqrDist);
    ClampAxis(pnt, i2, box, sqrDist);
}

float BoxLineSqrDistance(const CLine3& line, const CVolBox& box, float* t, float* b0, float* b1,
                         float* b2) {
    CMatrix m;
    WorldToBox(box, m);
    CVector3 pnt;
    m.TransformPoint(pnt, line.m_start);
    CVector3 dir;
    m.TransformVector(dir, line.m_dir);

    // Reflect so the direction has no negative components.
    bool reflectX = dir.x < 0.0f;
    if (reflectX) {
        dir.x = -dir.x;
        pnt.x = -pnt.x;
    }
    bool reflectY = dir.y < 0.0f;
    if (reflectY) {
        dir.y = -dir.y;
        pnt.y = -pnt.y;
    }
    bool reflectZ = dir.z < 0.0f;
    if (reflectZ) {
        dir.z = -dir.z;
        pnt.z = -pnt.z;
    }

    float sqrDist = 0.0f;
    if (dir.x > DIR_EPSILON) {
        if (dir.y > DIR_EPSILON) {
            if (dir.z > DIR_EPSILON) {
                // (+,+,+)
                CVector3 pmE;
                pmE.x = pnt.x - box.m_halfSize[0];
                pmE.y = pnt.y - box.m_halfSize[1];
                pmE.z = pnt.z - box.m_halfSize[2];
                if (dir.y * pmE.x >= dir.x * pmE.y) {
                    if (dir.z * pmE.x >= dir.x * pmE.z) {
                        Face(0, 1, 2, pnt, dir, box, pmE, t, sqrDist);
                    } else {
                        Face(2, 0, 1, pnt, dir, box, pmE, t, sqrDist);
                    }
                } else if (dir.z * pmE.y >= dir.y * pmE.z) {
                    Face(1, 2, 0, pnt, dir, box, pmE, t, sqrDist);
                } else {
                    Face(2, 0, 1, pnt, dir, box, pmE, t, sqrDist);
                }
            } else {
                // (+,+,0)
                Case0(0, 1, 2, pnt, dir, box, t, sqrDist);
            }
        } else if (dir.z > DIR_EPSILON) {
            // (+,0,+)
            Case0(0, 2, 1, pnt, dir, box, t, sqrDist);
        } else {
            // (+,0,0)
            Case00(0, 1, 2, pnt, dir, box, t, sqrDist);
        }
    } else if (dir.y > DIR_EPSILON) {
        if (dir.z > DIR_EPSILON) {
            // (0,+,+)
            Case0(1, 2, 0, pnt, dir, box, t, sqrDist);
        } else {
            // (0,+,0)
            Case00(1, 0, 2, pnt, dir, box, t, sqrDist);
        }
    } else if (dir.z > DIR_EPSILON) {
        // (0,0,+)
        Case00(2, 0, 1, pnt, dir, box, t, sqrDist);
    } else {
        // (0,0,0): the point
        ClampAxis(pnt, 0, box, sqrDist);
        ClampAxis(pnt, 1, box, sqrDist);
        ClampAxis(pnt, 2, box, sqrDist);
        if (t != NULL) {
            *t = 0.0f;
        }
    }

    if (t != NULL) {
        // Undo the reflections.
        if (reflectX) {
            pnt.x = -pnt.x;
        }
        if (reflectY) {
            pnt.y = -pnt.y;
        }
        if (reflectZ) {
            pnt.z = -pnt.z;
        }
        *b0 = pnt.x;
        *b1 = pnt.y;
        *b2 = pnt.z;
    }
    return sqrDist;
}

float BoxPointSqrDistance(const CVector3& point, const CVolBox& box, float* b0, float* b1, float* b2) {
    CMatrix m;
    WorldToBox(box, m);
    CVector3 pnt;
    m.TransformPoint(pnt, point);
    float sqrDist = 0.0f;
    ClampAxis(pnt, 0, box, sqrDist);
    ClampAxis(pnt, 1, box, sqrDist);
    ClampAxis(pnt, 2, box, sqrDist);
    if (b0 != NULL) {
        *b0 = pnt.x;
        *b1 = pnt.y;
        *b2 = pnt.z;
    }
    return sqrDist;
}

// The segment version (Magic Software's SqrDistance(Segment3, Box3)).
static inline float SegmentBoxSqrDistance(const CLine3& seg, const CVolBox& box, float* segParam,
                                          float* b0, float* b1, float* b2) {
    float lineParam, lb0, lb1, lb2;
    float sqrDist = BoxLineSqrDistance(seg, box, &lineParam, &lb0, &lb1, &lb2);
    if (lineParam >= 0.0f) {
        if (lineParam <= 1.0f) {
            *segParam = lineParam;
            *b0 = lb0;
            *b1 = lb1;
            *b2 = lb2;
            return sqrDist;
        }
        sqrDist = BoxPointSqrDistance(seg.m_end, box, b0, b1, b2);
        if (segParam != NULL) {
            *segParam = 1.0f;
        }
        return sqrDist;
    }
    sqrDist = BoxPointSqrDistance(seg.m_start, box, b0, b1, b2);
    if (segParam != NULL) {
        *segParam = 0.0f;
    }
    return sqrDist;
}

void CVolBox::GetExtents(CVector3& min, CVector3& max) const {
    float ex = m_halfSize[0];
    float ey = m_halfSize[1];
    float ez = m_halfSize[2];
    float xx = fabs(ex * m_axisX.x);
    float yx = fabs(ey * m_axisY.x);
    float zx = fabs(ez * m_axisZ.x);
    float yy = fabs(ey * m_axisY.y);
    float xy = fabs(ex * m_axisX.y);
    min.x = m_center.x - xx - yx - zx;
    float zy = fabs(ez * m_axisZ.y);
    float yz = fabs(ey * m_axisY.z);
    float xz = fabs(ex * m_axisX.z);
    float zz = fabs(ez * m_axisZ.z);
    min.y = m_center.y - xy - yy - zy;
    min.z = m_center.z - xz - yz - zz;
    max.x = m_center.x + xx + yx + zx;
    max.y = m_center.y + xy + yy + zy;
    max.z = m_center.z + xz + yz + zz;
}

bool CVolBox::TestVisibility(CDrawContext& ctx) const {
    static int remapCodes[6] = {0x20, 0x10, 0x2, 0x1, 0x4, 0x8};
    const int* code = remapCodes;
    const CPlane* plane = ((const CCamera*)ctx.m_pCamera)->m_frustum.m_planes;
    for (int i = 6; i != 0; i--) {
        float nx = plane->m_normal[0];
        float ny = plane->m_normal[1];
        float nz = plane->m_normal[2];
        CVector3 center = m_center;
        float ry = fabs(m_axisY.x * nx + m_axisY.y * ny + m_axisY.z * nz);
        float rx = fabs(m_axisX.x * nx + m_axisX.y * ny + m_axisX.z * nz);
        float radius = rx * m_halfSize[0] + ry * m_halfSize[1] +
                       (float)fabs(m_axisZ.x * nx + m_axisZ.y * ny + m_axisZ.z * nz) * m_halfSize[2];
        float dist = (nx * center.x + ny * center.y + nz * center.z) - plane->m_d;
        if (dist > radius) {
            return false;
        }
        if (dist >= -radius) {
            ctx.m_2dReject |= *code;
        }
        code++;
        plane++;
    }
    return true;
}

int CVolBox::TestCollision(const CTriangle&, CCollision&, bool) const {
    return 0;
}

// The deepest box corner against the plane (corners walked one edge at a
// time, each axis added twice).
int CVolBox::TestCollision(const CPlane& plane, CCollision& collision, bool record) const {
    float ex = m_halfSize[0];
    float ey = m_halfSize[1];
    float ez = m_halfSize[2];
    float xx = ex * m_axisX.x, xy = ex * m_axisX.y, xz = ex * m_axisX.z;
    float yx = ey * m_axisY.x, yy = ey * m_axisY.y, yz = ey * m_axisY.z;
    float zx = ez * m_axisZ.x, zy = ez * m_axisZ.y, zz = ez * m_axisZ.z;

    CVector3 corner = m_center;
    corner.x = corner.x - xx;
    corner.y = corner.y - xy;
    corner.z = corner.z - xz;
    corner.x = corner.x - yx;
    corner.y = corner.y - yy;
    corner.z = corner.z - yz;
    corner.x = corner.x - zx;
    corner.y = corner.y - zy;
    corner.z = corner.z - zz;

    SClsnContact contact;
    contact.m_normal.x = -plane.m_normal[0];
    contact.m_normal.y = -plane.m_normal[1];
    contact.m_normal.z = -plane.m_normal[2];

#define TEST_CORNER(first)                                                                  \
    {                                                                                       \
        CVector3 c = corner;                                                                \
        float nx = plane.m_normal[0];                                                       \
        float ny = plane.m_normal[1];                                                       \
        float nz = plane.m_normal[2];                                                       \
        float d = (nx * c.x + ny * c.y + nz * c.z) - plane.m_d;                             \
        CVector3 p;                                                                         \
        p.x = -d * nx;                                                                      \
        p.y = -d * ny;                                                                      \
        p.z = -d * nz;                                                                      \
        p.x = p.x + c.x;                                                                    \
        p.y = p.y + c.y;                                                                    \
        p.z = p.z + c.z;                                                                    \
        if (first || d < contact.m_depth) {                                                 \
            contact.m_depth = d;                                                            \
            contact.m_point.Copy(p);                                                        \
        }                                                                                   \
    }
#define STEP(ax, ay, az, op) \
    corner.x = corner.x op ax; \
    corner.y = corner.y op ay; \
    corner.z = corner.z op az; \
    corner.x = corner.x op ax; \
    corner.y = corner.y op ay; \
    corner.z = corner.z op az;

    TEST_CORNER(true);
    STEP(xx, xy, xz, +);
    TEST_CORNER(false);
    STEP(yx, yy, yz, +);
    TEST_CORNER(false);
    STEP(xx, xy, xz, -);
    TEST_CORNER(false);
    STEP(zx, zy, zz, +);
    TEST_CORNER(false);
    STEP(yx, yy, yz, -);
    TEST_CORNER(false);
    STEP(xx, xy, xz, +);
    TEST_CORNER(false);
    STEP(yx, yy, yz, +);
    TEST_CORNER(false);
#undef TEST_CORNER
#undef STEP

    int result = ClassifyContact(contact.m_depth);
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

// Clips the line's parameter range against one slab of the box.
static inline bool ClipSlab(CVector3 axis, CVector3 dir, CVector3 diff, float extent, float& tMin,
                            float& tMax, CVector3& nMin, CVector3& nMax) {
    float denom = dir.x * axis.x + dir.y * axis.y + dir.z * axis.z;
    float numer = diff.x * axis.x + diff.y * axis.y + diff.z * axis.z;
    if ((float)fabs(denom) > SLAB_EPSILON) {
        float inv = 1.0f / denom;
        float tFar = inv * (extent - numer);
        float tNear = inv * (-extent - numer);
        if (tNear < tFar) {
            if (tNear > tMin) {
                tMin = tNear;
                nMin.x = -axis.x;
                nMin.y = -axis.y;
                nMin.z = -axis.z;
            }
            if (tFar < tMax) {
                tMax = tFar;
                nMax.Copy(axis);
            }
        } else {
            if (tFar > tMin) {
                tMin = tFar;
                nMin.Copy(axis);
            }
            if (tNear < tMax) {
                tMax = tNear;
                nMax.x = -axis.x;
                nMax.y = -axis.y;
                nMax.z = -axis.z;
            }
        }
    } else if ((float)fabs(numer) > extent) {
        return false;
    }
    return tMin <= tMax && tMax >= 0.0f;
}

int CVolBox::TestCollision(const CLine3& line, CCollision& collision, bool record) const {
    CVector3 dir = line.m_dir;
    CVector3 start = line.m_start;
    CVector3 diff;
    diff.x = start.x - m_center.x;
    diff.y = start.y - m_center.y;
    diff.z = start.z - m_center.z;
    float tMin = -BIG_FLOAT;
    float tMax = BIG_FLOAT;
    CVector3 nMin;
    CVector3 nMax;
    if (!ClipSlab(m_axisX, dir, diff, m_halfSize[0], tMin, tMax, nMin, nMax)) {
        return Miss(collision, record);
    }
    if (!ClipSlab(m_axisY, dir, diff, m_halfSize[1], tMin, tMax, nMin, nMax)) {
        return Miss(collision, record);
    }
    if (!ClipSlab(m_axisZ, dir, diff, m_halfSize[2], tMin, tMax, nMin, nMax)) {
        return Miss(collision, record);
    }
    if (tMin > 1.0f || tMax < 0.0f) {
        return Miss(collision, record);
    }

    // Entering point (or the start, when it is inside).
    float t;
    CVector3 normal;
    if (tMin >= 0.0f) {
        t = tMin;
        normal.Copy(nMin);
    } else {
        t = 0.0f;
        normal.Copy(nMax);
    }
    collision.m_fraction = t;

    int result;
    float distSq;
    if (collision.m_flags & CLSN_FLAG_NEAREST) {
        distSq = 0.0f;
        result = CLSN_TOUCHING;
    } else {
        // Touching only when the hit is the segment's end point.
        float rest = 1.0f - t;
        float ry = rest * dir.y;
        float rx = rest * dir.x;
        float rz = rest * dir.z;
        distSq = rx * rx + ry * ry + rz * rz;
        if (-distSq > CONTACT_EPSILON_SQRD) {
            result = CLSN_NONE;
        } else if (distSq > CONTACT_EPSILON_SQRD) {
            result = CLSN_PENETRATING;
        } else {
            result = CLSN_TOUCHING;
        }
    }
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            CVector3 lineStart = line.m_start;
            CVector3 hit;
            hit.x = t * dir.x;
            hit.y = t * dir.y;
            hit.z = t * dir.z;
            hit.x = hit.x + lineStart.x;
            hit.y = hit.y + lineStart.y;
            hit.z = hit.z + lineStart.z;
            SClsnContact contact;
            contact.m_depth = sqrtf(distSq);
            contact.m_point.Copy(hit);
            contact.m_normal.Copy(normal);
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolBox::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

// A fiber is a line that only reports its nearest hit.
int CVolBox::TestCollision(const CVolFiber& fiber, CCollision& collision, bool record) const {
    collision.m_flags |= CLSN_FLAG_NEAREST;
    return TestCollision(fiber.GetLine(), collision, record);
}

int CVolBox::TestCollision(CVector3 point, CCollision& collision, bool record) const {
    CVolSphere sphere(point, CONTACT_EPSILON);
    return TestCollision(sphere, collision, record);
}

int CVolBox::TestCollision(const CCSGVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolBox::TestCollision(const CWorldVolume& volume, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)volume).TestCollision(*this, collision, record);
}

int CVolBox::TestCollision(const CCDBObject& object, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return ((const IVolume&)object).TestCollision(*this, collision, record);
}

// True when the capsule axis (start + t dir, t in [0, 1], swollen by
// `radius`) lies entirely outside the box's slab along `axis`.
static inline bool CapsuleOutsideSlab(CVector3 axis, CVector3 dir, CVector3 diff, float extent,
                                      float radius) {
    float d = dir.x * axis.x + dir.y * axis.y + dir.z * axis.z;
    float s = diff.x * axis.x + diff.y * axis.y + diff.z * axis.z;
    if ((float)fabs(d) > CAPSULE_AXIS_EPSILON) {
        if (d > 0.0f) {
            if (s > 0.0f) {
                return s - radius > extent;
            }
            return radius + (s + d) < -extent;
        }
        if (s > 0.0f) {
            return (s + d) - radius > extent;
        }
        return s + radius < -extent;
    }
    return (float)fabs(s) - radius > extent;
}

int CVolBox::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool record) const {
    OSGetTick();
    const CLine3& axisLine = capsule.m_line;
    CVector3 dir = axisLine.m_dir;
    CVector3 start = axisLine.m_start;
    CVector3 diff;
    diff.x = start.x - m_center.x;
    diff.y = start.y - m_center.y;
    diff.z = start.z - m_center.z;
    float radius = CONTACT_EPSILON + capsule.m_radius;
    int result;

    if (CapsuleOutsideSlab(m_axisX, dir, diff, m_halfSize[0], radius) ||
        CapsuleOutsideSlab(m_axisY, dir, diff, m_halfSize[1], radius) ||
        CapsuleOutsideSlab(m_axisZ, dir, diff, m_halfSize[2], radius)) {
        if (record && collision.m_result < CLSN_NONE) {
            collision.m_result = CLSN_NONE;
        }
        result = CLSN_NONE;
    } else {
        // Separated along the line from the box center to the axis?
        CVector3 closest;
        float t;
        axisLine.GetProjection(m_center, closest, t);
        float vx = closest.x - m_center.x;
        float vy = closest.y - m_center.y;
        float vz = closest.z - m_center.z;
        float distSq = vx * vx + vy * vy + vz * vz;
        bool separated = false;
        if (distSq > radius * radius) {
            float dist = sqrtf(distSq);
            float inv = 1.0f / dist;
            float ny = vy * inv;
            float nx = vx * inv;
            float nz = vz * inv;
            float ex = m_halfSize[0];
            float ey = m_halfSize[1];
            float ez = m_halfSize[2];
            float boxRadius =
                (float)fabs(ex * m_axisX.x * nx + ex * m_axisX.y * ny + ex * m_axisX.z * nz) +
                (float)fabs(ey * m_axisY.x * nx + ey * m_axisY.y * ny + ey * m_axisY.z * nz) +
                (float)fabs(ez * m_axisZ.x * nx + ez * m_axisZ.y * ny + ez * m_axisZ.z * nz);
            if (dist - boxRadius > radius) {
                separated = true;
            }
        }
        if (separated) {
            if (record && collision.m_result < CLSN_NONE) {
                collision.m_result = CLSN_NONE;
            }
            result = CLSN_NONE;
        } else {
            float segT, b0, b1, b2;
            float sqrDist = SegmentBoxSqrDistance(axisLine, *this, &segT, &b0, &b1, &b2);
            float depth = sqrtf(sqrDist) - capsule.m_radius;
            result = ClassifyContact(depth);
            if (record) {
                if (result > collision.m_result) {
                    collision.m_result = result;
                }
                if (result == CLSN_TOUCHING) {
                    CVector3 segStart = axisLine.m_start;
                    CVector3 segDir = axisLine.m_dir;
                    float px = segStart.x + segT * segDir.x;
                    float py = segStart.y + segT * segDir.y;
                    float pz = segStart.z + segT * segDir.z;
                    CVector3 normal;
                    normal.x = px - (m_center.x + b0 * m_axisX.x + b1 * m_axisY.x + b2 * m_axisZ.x);
                    normal.y = py - (m_center.y + b0 * m_axisX.y + b1 * m_axisY.y + b2 * m_axisZ.y);
                    normal.z = pz - (m_center.z + b0 * m_axisX.z + b1 * m_axisY.z + b2 * m_axisZ.z);
                    normal.Normalize();
                    float r = -capsule.m_radius;
                    SClsnContact contact;
                    contact.m_depth = depth;
                    contact.m_normal.Copy(normal);
                    contact.m_point.x = px + r * normal.x;
                    contact.m_point.y = py + r * normal.y;
                    contact.m_point.z = pz + r * normal.z;
                    contact.m_pPoly = NULL;
                    collision.AddContact(contact);
                }
            }
        }
    }
    return result;
}

// -1, 0 or 1 by the sign bit (denormals count as zero).
static float Sign(float x) {
    union {
        float f;
        u32 i;
    } v;
    v.f = x;
    if (v.i & 0x7F800000) {
        v.i = (v.i & 0x80000000) | 0x3F800000;
    } else {
        v.f = 0.0f;
    }
    return v.f;
}

int CVolBox::TestCollision(const CVolSphere& sphere, CCollision& collision, bool record) const {
    OSGetTick();
    CVector3 center = sphere.GetCenter();
    float dy = center.y - m_center.y;
    float dx = center.x - m_center.x;
    float dz = center.z - m_center.z;
    bool outside = false;
    float lz = m_axisZ.x * dx + m_axisZ.y * dy + m_axisZ.z * dz;
    float maxNeg = -BIG_FLOAT;
    float lx = m_axisX.x * dx + m_axisX.y * dy + m_axisX.z * dz;
    float ly = m_axisY.x * dx + m_axisY.y * dy + m_axisY.z * dz;
    float sqrDist = 0.0f;
    float ax = fabs(lx);
    float ay = fabs(ly);
    float az = fabs(lz);
    CVector3 local;
    local.x = lx;
    local.y = ly;
    local.z = lz;

    // Box-space normal and closest point (only built when recording).
    float nx, ny, nz;
    float d;
    if (record) {
        nx = 0.0f;
        ny = 0.0f;
        nz = 0.0f;
        CVector3 point = local;
        d = ax - m_halfSize[0];
        if (d > 0.0f) {
            sqrDist += d * d;
            outside = true;
            float s = Sign(lx);
            nx = nx + s * d;
            ny = ny + 0.0f;
            nz = nz + 0.0f;
            point.x = s * m_halfSize[0];
        } else {
            maxNeg = d;
        }
        d = ay - m_halfSize[1];
        if (d > 0.0f) {
            sqrDist += d * d;
            outside = true;
            float s = Sign(ly);
            nx = nx + 0.0f;
            ny = ny + s * d;
            nz = nz + 0.0f;
            point.y = s * m_halfSize[1];
        } else if (d > maxNeg) {
            maxNeg = d;
        }
        d = az - m_halfSize[2];
        if (d > 0.0f) {
            sqrDist += d * d;
            outside = true;
            float s = Sign(lz);
            nx = nx + 0.0f;
            ny = ny + 0.0f;
            nz = nz + s * d;
            point.z = s * m_halfSize[2];
        } else if (d > maxNeg) {
            maxNeg = d;
        }
        local = point;
    } else {
        d = ax - m_halfSize[0];
        if (d > 0.0f) {
            sqrDist += d * d;
            outside = true;
        } else {
            maxNeg = d;
        }
        d = ay - m_halfSize[1];
        if (d > 0.0f) {
            sqrDist += d * d;
            outside = true;
        } else if (d > maxNeg) {
            maxNeg = d;
        }
        d = az - m_halfSize[2];
        if (d > 0.0f) {
            sqrDist += d * d;
            outside = true;
        } else if (d > maxNeg) {
            maxNeg = d;
        }
    }

    float depth;
    if (outside) {
        depth = sqrtf(sqrDist) - sphere.GetRadius();
    } else {
        depth = maxNeg - sphere.GetRadius();
    }
    int result = ClassifyContact(depth);
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            // Back to world space.
            SClsnContact contact;
            contact.m_depth = depth;
            contact.m_normal.x = m_axisX.x * nx + m_axisY.x * ny + m_axisZ.x * nz;
            contact.m_normal.y = m_axisX.y * nx + m_axisY.y * ny + m_axisZ.y * nz;
            contact.m_normal.z = m_axisX.z * nx + m_axisY.z * ny + m_axisZ.z * nz;
            contact.m_normal.Normalize();
            contact.m_pPoly = NULL;
            float pz = m_axisX.z * local.x + m_axisY.z * local.y + m_axisZ.z * local.z;
            float px = m_axisX.x * local.x + m_axisY.x * local.y + m_axisZ.x * local.z;
            float py = m_axisX.y * local.x + m_axisY.y * local.y + m_axisZ.y * local.z;
            contact.m_point.x = px + m_center.x;
            contact.m_point.y = py + m_center.y;
            contact.m_point.z = pz + m_center.z;
            collision.AddContact(contact);
        }
    }
    OSGetTick();
    return result;
}

// One separating-axis test: false when the boxes are apart along the axis;
// otherwise keeps the axis with the smallest separation.
static bool BoxBoxTest(float radius, float dist, EAxis axis, float& minSep, float& sign,
                       EAxis& bestAxis) {
    float sep = (float)fabs(dist) - radius;
    if (ClassifyContact(sep) != CLSN_NONE) {
        if (sep < minSep) {
            minSep = sep;
            sign = Sign(dist);
            bestAxis = axis;
        }
        return true;
    }
    return false;
}

// Box-space coordinates of the contact point for edge pair Ai x Bj (Eberly,
// "Dynamic Collision Detection using Oriented Bounding Boxes"). The two
// coordinates other than i are box A's extents; coordinate i is solved.
static inline void EdgeEdgePoint(int i, int j, const float c[3][3], const float* dA,
                                 const float* dB, const CVolBox& a, const CVolBox& b, float s,
                                 CVector3& local) {
    int i1 = (i + 1) % 3;
    int i2 = (i + 2) % 3;
    int j1 = (j + 1) % 3;
    int j2 = (j + 2) % 3;
    float x[3];
    float y[3];
    x[i1] = a.m_halfSize[i1] * (-s * Sign(c[i2][j]));
    x[i2] = a.m_halfSize[i2] * (s * Sign(c[i1][j]));
    y[j1] = b.m_halfSize[j1] * (-s * Sign(c[i][j2]));
    y[j2] = b.m_halfSize[j2] * (s * Sign(c[i][j1]));
    // Sums in index order, as the original does.
    int lo = i1 < i2 ? i1 : i2;
    int hi = i1 < i2 ? i2 : i1;
    int jlo = j1 < j2 ? j1 : j2;
    int jhi = j1 < j2 ? j2 : j1;
    float denom = 1.0f - c[i][j] * c[i][j];
    if ((float)fabs(denom) > EDGE_EPSILON) {
        x[i] = (dA[i] + c[i][j] * ((c[lo][j] * x[lo] + c[hi][j] * x[hi]) - dB[j]) +
                c[i][jlo] * y[jlo] + c[i][jhi] * y[jhi]) /
               denom;
    } else {
        x[i] = 0.0f;
    }
    local.x = x[0];
    local.y = x[1];
    local.z = x[2];
}

int CVolBox::TestCollision(const CVolBox& other, CCollision& collision, bool record) const {
    const CMatrix& frameA = *(const CMatrix*)&m_axisX;
    const CMatrix& frameB = *(const CMatrix*)&other.m_axisX;
    CVector3 d;
    d.Sub(other.m_center, m_center);
    EAxis bestAxis = AXIS_NONE;
    float minSep = BIG_FLOAT;
    float sign;
    const CVector3* axesA[3] = {&m_axisX, &m_axisY, &m_axisZ};
    const CVector3* axesB[3] = {&other.m_axisX, &other.m_axisY, &other.m_axisZ};

    // c[i][j] = Ai . Bj, absC = |c|; dA, dB: center offset in each frame.
    CVector3 v;
    v.DotDotDot(m_axisX, m_axisY, m_axisZ, d);
    float dA[3] = {v.x, v.y, v.z};
    v.DotDotDot(other.m_axisX, other.m_axisY, other.m_axisZ, d);
    float dB[3] = {v.x, v.y, v.z};
    float c[3][3];
    float absC[3][3];
    for (int i = 0; i < 3; i++) {
        v.DotDotDot(other.m_axisX, other.m_axisY, other.m_axisZ, *axesA[i]);
        c[i][0] = v.x;
        c[i][1] = v.y;
        c[i][2] = v.z;
        v.Abs(v);
        absC[i][0] = v.x;
        absC[i][1] = v.y;
        absC[i][2] = v.z;
    }
    const float* eA = m_halfSize;
    const float* eB = other.m_halfSize;

#define SEPARATED(r, dist, axis) !BoxBoxTest(r, dist, axis, minSep, sign, bestAxis)
#define EDGE_SEPARATED(r, dist, axis) ((dist) != 0.0f && SEPARATED(r, dist, axis))
    if (SEPARATED(eA[0] + (eB[0] * absC[0][0] + eB[1] * absC[0][1] + eB[2] * absC[0][2]), dA[0], AXIS_A0) ||
        SEPARATED(eA[1] + (eB[0] * absC[1][0] + eB[1] * absC[1][1] + eB[2] * absC[1][2]), dA[1], AXIS_A1) ||
        SEPARATED(eA[2] + (eB[0] * absC[2][0] + eB[1] * absC[2][1] + eB[2] * absC[2][2]), dA[2], AXIS_A2) ||
        SEPARATED((eA[0] * absC[0][0] + eA[1] * absC[1][0] + eA[2] * absC[2][0]) + eB[0], dB[0], AXIS_B0) ||
        SEPARATED((eA[0] * absC[0][1] + eA[1] * absC[1][1] + eA[2] * absC[2][1]) + eB[1], dB[1], AXIS_B1) ||
        SEPARATED((eA[0] * absC[0][2] + eA[1] * absC[1][2] + eA[2] * absC[2][2]) + eB[2], dB[2], AXIS_B2) ||
        EDGE_SEPARATED((eA[1] * absC[2][0] + eA[2] * absC[1][0]) + (eB[1] * absC[0][2] + eB[2] * absC[0][1]),
                       c[1][0] * dA[2] - c[2][0] * dA[1], AXIS_A0B0) ||
        EDGE_SEPARATED((eA[1] * absC[2][1] + eA[2] * absC[1][1]) + (eB[0] * absC[0][2] + eB[2] * absC[0][0]),
                       c[1][1] * dA[2] - c[2][1] * dA[1], AXIS_A0B1) ||
        EDGE_SEPARATED((eA[1] * absC[2][2] + eA[2] * absC[1][2]) + (eB[0] * absC[0][1] + eB[1] * absC[0][0]),
                       c[1][2] * dA[2] - c[2][2] * dA[1], AXIS_A0B2) ||
        EDGE_SEPARATED((eA[0] * absC[2][0] + eA[2] * absC[0][0]) + (eB[1] * absC[1][2] + eB[2] * absC[1][1]),
                       c[2][0] * dA[0] - c[0][0] * dA[2], AXIS_A1B0) ||
        EDGE_SEPARATED((eA[0] * absC[2][1] + eA[2] * absC[0][1]) + (eB[0] * absC[1][2] + eB[2] * absC[1][0]),
                       c[2][1] * dA[0] - c[0][1] * dA[2], AXIS_A1B1) ||
        EDGE_SEPARATED((eA[0] * absC[2][2] + eA[2] * absC[0][2]) + (eB[0] * absC[1][1] + eB[1] * absC[1][0]),
                       c[2][2] * dA[0] - c[0][2] * dA[2], AXIS_A1B2) ||
        EDGE_SEPARATED((eA[0] * absC[1][0] + eA[1] * absC[0][0]) + (eB[1] * absC[2][2] + eB[2] * absC[2][1]),
                       c[0][0] * dA[1] - c[1][0] * dA[0], AXIS_A2B0) ||
        EDGE_SEPARATED((eA[0] * absC[1][1] + eA[1] * absC[0][1]) + (eB[0] * absC[2][2] + eB[2] * absC[2][0]),
                       c[0][1] * dA[1] - c[1][1] * dA[0], AXIS_A2B1) ||
        EDGE_SEPARATED((eA[0] * absC[1][2] + eA[1] * absC[0][2]) + (eB[0] * absC[2][1] + eB[1] * absC[2][0]),
                       c[0][2] * dA[1] - c[1][2] * dA[0], AXIS_A2B2)) {
        return Miss(collision, record);
    }
#undef SEPARATED
#undef EDGE_SEPARATED

    int result = ClassifyContact(minSep);
    if (record) {
        if (result > collision.m_result) {
            collision.m_result = result;
        }
        if (result == CLSN_TOUCHING) {
            SClsnContact contact;
            contact.m_depth = minSep;
            CVector3 local;
            switch (bestAxis) {
            case AXIS_A0:
            case AXIS_A1:
            case AXIS_A2: {
                // Face of A: the vertex of B furthest along -normal.
                int i = bestAxis - AXIS_A0;
                contact.m_normal.Scale(*axesA[i], sign);
                local.x = eB[0] * (-sign * Sign(c[i][0]));
                local.y = eB[1] * (-sign * Sign(c[i][1]));
                local.z = eB[2] * (-sign * Sign(c[i][2]));
                frameB.TransformPoint(contact.m_point, local);
                break;
            }
            case AXIS_B0:
            case AXIS_B1:
            case AXIS_B2: {
                // Face of B: the vertex of A furthest along the normal.
                int j = bestAxis - AXIS_B0;
                contact.m_normal.Scale(*axesB[j], sign);
                local.x = eA[0] * (sign * Sign(c[0][j]));
                local.y = eA[1] * (sign * Sign(c[1][j]));
                local.z = eA[2] * (sign * Sign(c[2][j]));
                frameA.TransformPoint(contact.m_point, local);
                break;
            }
            case AXIS_A0B0:
            case AXIS_A0B1:
            case AXIS_A0B2:
            case AXIS_A1B0:
            case AXIS_A1B1:
            case AXIS_A1B2:
            case AXIS_A2B0:
            case AXIS_A2B1:
            case AXIS_A2B2: {
                int i = (bestAxis - AXIS_A0B0) / 3;
                int j = (bestAxis - AXIS_A0B0) % 3;
                contact.m_normal.Cross(*axesA[i], *axesB[j]);
                contact.m_normal.Normalize();
                contact.m_normal.x *= sign;
                contact.m_normal.y *= sign;
                contact.m_normal.z *= sign;
                EdgeEdgePoint(i, j, c, dA, dB, *this, other, sign, local);
                frameA.TransformPoint(contact.m_point, local);
                break;
            }
            }
            contact.m_pPoly = NULL;
            collision.AddContact(contact);
        }
    }
    return result;
}

int CVolBox::TestCollision(const IVolume& other, CCollision& collision, bool record) const {
    collision.SwapOrder();
    return other.TestCollision(*this, collision, record);
}

float CVolBox::GetDepth() const {
    return 2.0f * m_halfSize[1];
}

float CVolBox::GetWidth() const {
    return 2.0f * m_halfSize[0];
}

void CVolBox::SetCorners(CVector3 a, CVector3 b) {
    m_center.x = b.x + a.x;
    m_center.y = b.y + a.y;
    m_center.z = b.z + a.z;
    m_center.x = m_center.x * 0.5f;
    m_center.y = m_center.y * 0.5f;
    m_center.z = m_center.z * 0.5f;
    float dy = m_center.y - a.y;
    float dx = m_center.x - a.x;
    float dz = m_center.z - a.z;
    float ey = fabs(m_axisY.x * dx + m_axisY.y * dy + m_axisY.z * dz);
    float ez = fabs(m_axisZ.x * dx + m_axisZ.y * dy + m_axisZ.z * dz);
    m_halfSize[0] = fabs(m_axisX.x * dx + m_axisX.y * dy + m_axisX.z * dz);
    m_halfSize[1] = ey;
    m_halfSize[2] = ez;
}

void CVolBox::SetDepth(float depth) {
    m_halfSize[1] = 0.5f * depth;
}

void CVolBox::SetHeight(float height) {
    m_halfSize[2] = 0.5f * height;
}

void CVolBox::SetWidth(float width) {
    m_halfSize[0] = 0.5f * width;
}

void CVolBox::SetBasis(CVector3 x, CVector3 y, CVector3 z) {
    m_axisX.Copy(x);
    m_axisY.Copy(y);
    m_axisZ.Copy(z);
}

void CVolBox::SetCenter(CVector3 center) {
    m_center.Copy(center);
}

void CVolBox::TransformedCopy(const IVolume& volume, const CMatrix& m) {
    const CVolBox& src = (const CVolBox&)volume;
    m_halfSize[0] = src.m_halfSize[0];
    m_halfSize[1] = src.m_halfSize[1];
    m_halfSize[2] = src.m_halfSize[2];
    m.TransformVector(m_axisX, src.m_axisX);
    m.TransformVector(m_axisY, src.m_axisY);
    m.TransformVector(m_axisZ, src.m_axisZ);
    m.TransformPoint(m_center, src.m_center);
}

int CVolBox::Create() const {
    return (int)PTR_TO_ADDR(new CVolBox);
}

void CVolBox::operator=(const CVolBox& other) {
    if (&other != this) {
        m_halfSize[0] = other.m_halfSize[0];
        m_halfSize[1] = other.m_halfSize[1];
        m_halfSize[2] = other.m_halfSize[2];
        m_axisX.Copy(other.m_axisX);
        m_axisY.Copy(other.m_axisY);
        m_axisZ.Copy(other.m_axisZ);
        m_center.Copy(other.m_center);
    }
}

CVolBox::~CVolBox() {}
