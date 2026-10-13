// Spline paths for AI walks: each level poly path becomes a natural cubic
// spline through its points (forwards and backwards), split into one cubic
// segment per pair of points. A traversal keeps a walker heading for a
// point a little ahead of the closest point on the curve.
#include "game/AISplinePath.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/bsobject.h"
#include "game/memory.h"
#include "ea/real.h"
#include "game/vector.h"
#include "math.h"

// Events sent to the walker's script.
#define SPLINEEVT_ON_PATH 0x36    // reached the spline (after the A* walk to it)
#define SPLINEEVT_DONE 0x37       // reached the end
#define SPLINEEVT_NEXT_SEGMENT 0xA1

static inline BSObject* WalkerScript(CAIFilterObject* filter) {
    return ((CAIFilterSoldierObject*)filter)->m_pBSObject;
}

// P(t) of `seg` into out[3].
static inline void Evaluate(const CAISplinePathSegment* seg, float t, f32* out) {
    out[0] = t * seg->m_a.x;
    out[1] = t * seg->m_a.y;
    out[2] = t * seg->m_a.z;
    out[0] += seg->m_b.x;
    out[1] += seg->m_b.y;
    out[2] += seg->m_b.z;
    out[0] *= t;
    out[1] *= t;
    out[2] *= t;
    out[0] += seg->m_c.x;
    out[1] += seg->m_c.y;
    out[2] += seg->m_c.z;
    out[0] *= t;
    out[1] *= t;
    out[2] *= t;
    out[0] += seg->m_d.x;
    out[1] += seg->m_d.y;
    out[2] += seg->m_d.z;
}

CAISplinePathSegment::CAISplinePathSegment() {}

CAISplinePathSegment::~CAISplinePathSegment() {}

void CAISplinePathSegment::Expand(float t, CVector3& out) {
    Evaluate(this, t, &out.x);
}

// P'(t) = 3a t^2 + 2b t + c.
void CAISplinePathSegment::ExpandDerivative(float t, CVector3& out) {
    float t15 = 1.5f * t;
    float t2 = 2.0f * t;
    out.x = t15 * m_a.x;
    out.y = t15 * m_a.y;
    out.z = t15 * m_a.z;
    out.x += m_b.x;
    out.y += m_b.y;
    out.z += m_b.z;
    out.x *= t2;
    out.y *= t2;
    out.z *= t2;
    out.x += m_c.x;
    out.y += m_c.y;
    out.z += m_c.z;
}

static inline float Cubic(float t, float a, float b, float c, float d) {
    return ((a * t + b) * t + c) * t + d;
}

static inline float DistSqTo(const CAISplinePathSegment* s, float t, const CAIFilterRealVector3& pos) {
    float dy = Cubic(t, s->m_a.y, s->m_b.y, s->m_c.y, s->m_d.y) - pos.field_0x4;
    float dx = Cubic(t, s->m_a.x, s->m_b.x, s->m_c.x, s->m_d.x) - pos.field_0x0;
    float dz = Cubic(t, s->m_a.z, s->m_b.z, s->m_c.z, s->m_d.z) - pos.field_0x8;
    return dz * dz + (dx * dx + dy * dy);
}

// The parameter (from `start` on) of the point closest to `pos`: Newton's
// method on the distance, else a search in steps of m_step.
float CAISplinePathSegment::GetClosestParameter(float start, const CAIFilterRealVector3& pos) {
    float t = start;
    for (int i = 8; i != 0; i--) {
        float t15 = 1.5f * t;
        float t3 = 3.0f * t;
        float t2 = 2.0f * t;
        float dy = Cubic(t, m_a.y, m_b.y, m_c.y, m_d.y) - pos.field_0x4;
        float dx = Cubic(t, m_a.x, m_b.x, m_c.x, m_d.x) - pos.field_0x0;
        float dz = Cubic(t, m_a.z, m_b.z, m_c.z, m_d.z) - pos.field_0x8;
        float vy = (t15 * m_a.y + m_b.y) * t2 + m_c.y;
        float vx = (t15 * m_a.x + m_b.x) * t2 + m_c.x;
        float vz = (t15 * m_a.z + m_b.z) * t2 + m_c.z;
        float num = dz * vz + (dx * vx + dy * vy);
        float den = (vz * vz + (vx * vx + vy * vy)) +
                    (dz * ((t3 * m_a.z + m_b.z) * 2.0f) +
                     (dx * ((t3 * m_a.x + m_b.x) * 2.0f) + dy * ((t3 * m_a.y + m_b.y) * 2.0f)));
        if (!((float)fabs(den) > (float)fabs(0.0001f * num))) {
            break;
        }
        float next = t - num / den;
        if (fabs(next - t) < 0.125f * m_step) {
            if (next >= start && next < 1.0f) {
                return next;
            }
            break;
        }
        t = next;
    }
    // Brute force over at most 16 steps.
    float best = start;
    float bestDist = DistSqTo(this, start, pos);
    float limit1 = 1.0f + m_step;
    float limit2 = 16.0f * m_step + start;
    float* limit = limit1 < limit2 ? &limit1 : &limit2;
    float step = m_step;
    for (float s = start + step; s < *limit; s += step) {
        float d = DistSqTo(this, s, pos);
        if (d < bestDist) {
            best = s;
            bestDist = d;
        }
    }
    return best;
}

CAISplinePath::CAISplinePath() {}

CAISplinePath::~CAISplinePath() {}

// Builds the segments through `numPoints` points (`reverse`: last to
// first): a natural cubic spline (tangents from the tridiagonal system),
// one Hermite segment per pair, and a final zero-length segment at the end.
void CAISplinePath::GenerateTestSplinePath(const CAISplinePathManager& mgr, unsigned long numPoints,
                                           const _aifilter_simple_v3* points, bool reverse) {
    AISplineVec* p = mgr.m_points;
    f32* gamma = mgr.m_gamma;
    AISplineVec* delta = mgr.m_delta;
    u32 last = numPoints - 1;
    AISplineVec* D = mgr.m_tangents;
    m_lastSegment = last;
    m_segments = new CAISplinePathSegment[numPoints];
    u32 i;
    if (reverse) {
        for (i = 0; i < numPoints; i++) {
            const _aifilter_simple_v3* src = &points[numPoints - i - 1];
            p[i].x = src->x;
            p[i].y = src->y;
            p[i].z = src->z;
        }
    } else {
        for (i = 0; i < numPoints; i++) {
            p[i].x = points[i].x;
            p[i].y = points[i].y;
            p[i].z = points[i].z;
        }
    }

    // Forward elimination.
    gamma[0] = 0.5f;
    for (i = 1; i < last; i++) {
        gamma[i] = 1.0f / (4.0f - gamma[i - 1]);
    }
    gamma[last] = 1.0f / (2.0f - gamma[last - 1]);
    delta[0].x = p[1].x - p[0].x;
    delta[0].y = p[1].y - p[0].y;
    delta[0].z = p[1].z - p[0].z;
    float g = 3.0f * gamma[0];
    delta[0].x *= g;
    delta[0].y *= g;
    delta[0].z *= g;
    for (i = 1; i < last; i++) {
        delta[i].x = p[i + 1].x - p[i - 1].x;
        delta[i].y = p[i + 1].y - p[i - 1].y;
        delta[i].z = p[i + 1].z - p[i - 1].z;
        delta[i].x *= 3.0f;
        delta[i].y *= 3.0f;
        delta[i].z *= 3.0f;
        delta[i].x -= delta[i - 1].x;
        delta[i].y -= delta[i - 1].y;
        delta[i].z -= delta[i - 1].z;
        delta[i].x *= gamma[i];
        delta[i].y *= gamma[i];
        delta[i].z *= gamma[i];
    }
    delta[last].x = p[last].x - p[last - 1].x;
    delta[last].y = p[last].y - p[last - 1].y;
    delta[last].z = p[last].z - p[last - 1].z;
    delta[last].x *= 3.0f;
    delta[last].y *= 3.0f;
    delta[last].z *= 3.0f;
    delta[last].x -= delta[last - 1].x;
    delta[last].y -= delta[last - 1].y;
    delta[last].z -= delta[last - 1].z;
    delta[last].x *= gamma[last];
    delta[last].y *= gamma[last];
    delta[last].z *= gamma[last];

    // Back substitution.
    D[last] = delta[last];
    for (u32 j = last - 1; j != (u32)-1; j--) {
        float m = -gamma[j];
        D[j].x = m * D[j + 1].x;
        D[j].y = m * D[j + 1].y;
        D[j].z = m * D[j + 1].z;
        D[j].x += delta[j].x;
        D[j].y += delta[j].y;
        D[j].z += delta[j].z;
    }

    AISplineVec zero;
    zero.z = 0.0f;
    zero.y = 0.0f;
    zero.x = 0.0f;
    for (i = 0; i < last; i++) {
        AISplineVec* p0 = &p[i];
        AISplineVec* p1 = &p[i + 1];
        AISplineVec* d0 = &D[i];
        AISplineVec* d1 = &D[i + 1];
        AISplineVec a;
        AISplineVec b;
        AISplineVec c = *d0;
        AISplineVec d = *p0;
        // a = 2 (p0 - p1) + D0 + D1, b = 3 (p1 - p0) - 2 D0 - D1
        a.x = (p0->x - p1->x) * 2.0f + d0->x + d1->x;
        a.y = (p0->y - p1->y) * 2.0f + d0->y + d1->y;
        a.z = (p0->z - p1->z) * 2.0f + d0->z + d1->z;
        b.x = (p1->x - p0->x) * 3.0f - (2.0f * d0->x + d1->x);
        b.y = (p1->y - p0->y) * 3.0f - (2.0f * d0->y + d1->y);
        b.z = (p1->z - p0->z) * 3.0f - (2.0f * d0->z + d1->z);
        // The integral of |P'|^2 over the segment: about its length squared.
        float lenSq = (c.z * c.z + (c.x * c.x + c.y * c.y)) +
                      (2.0f * (b.z * c.z + (b.x * c.x + b.y * c.y)) +
                       (1.3333333f * (b.z * b.z + (b.x * b.x + b.y * b.y)) +
                        (2.0f * (a.z * c.z + (a.x * c.x + a.y * c.y)) +
                         (1.8f * (a.z * a.z + (a.x * a.x + a.y * a.y)) +
                          3.0f * (a.z * b.z + (a.x * b.x + a.y * b.y))))));
        CAISplinePathSegment* seg = &m_segments[i];
        if (lenSq < 1.0f) {
            // Too short to bother: a straight line.
            seg->m_step = 0.25f;
            seg->m_a = zero;
            seg->m_b = zero;
            seg->m_c.x = p1->x - p0->x;
            seg->m_c.y = p1->y - p0->y;
            seg->m_c.z = p1->z - p0->z;
            seg->m_d = d;
        } else {
            seg->m_step = 0.25f / sqrtf(lenSq);
            seg->m_a = a;
            seg->m_b = b;
            seg->m_c = c;
            seg->m_d = d;
        }
    }
    CAISplinePathSegment* end = &m_segments[last];
    end->m_step = 0.25f;
    end->m_a = zero;
    end->m_b = zero;
    end->m_c = D[last];
    end->m_d = p[last];
    for (i = 0; i < numPoints; i++) {
        m_segments[i].m_pNext = &m_segments[i + 1];
    }
    m_segments[last].m_pNext = NULL;
}

CAISplinePathTraversal::CAISplinePathTraversal() {
    m_state = SPLINE_IDLE;
    m_path = NULL;
}

CAISplinePathTraversal::~CAISplinePathTraversal() {}

// Aims 4 steps ahead of m_param; past the end, a quarter unit beyond the
// end along the curve.
void CAISplinePathTraversal::MakeModifiedNextPoint() {
    CAISplinePathSegment* seg = m_segment;
    float t = 4.0f * seg->m_step + m_param;
    if (t > 1.0f) {
        m_param = 1.0f;
        Evaluate(m_segment, 1.0f, m_point);
        CAISplinePathSegment* s = m_segment;
        float vy = (1.5f * s->m_a.y + s->m_b.y) * 2.0f + s->m_c.y;
        float vx = (1.5f * s->m_a.x + s->m_b.x) * 2.0f + s->m_c.x;
        float vz = (1.5f * s->m_a.z + s->m_b.z) * 2.0f + s->m_c.z;
        float len = vz * vz + (vx * vx + vy * vy);
        if (len > 0.0f) {
            len = sqrtf(len);
        }
        if (len != 0.0f) {
            float inv = 1.0f / len;
            vx *= inv;
            vy *= inv;
            vz *= inv;
        }
        m_point[0] += vx * 0.25f;
        m_point[1] += vy * 0.25f;
        m_point[2] += vz * 0.25f;
        return;
    }
    Evaluate(seg, t, m_point);
}

// Moves on to the first following segment that has a point closest to `pos`.
void CAISplinePathTraversal::SetupNextSegmentForward(const CAIFilterRealPosition& pos) {
    float t = m_segment->GetClosestParameter(0.0f, *(const CAIFilterRealVector3*)&pos);
    while (t < 0.0f || t > 1.0f) {
        m_segmentIndex++;
        m_segment++;
        t = m_segment->GetClosestParameter(0.0f, *(const CAIFilterRealVector3*)&pos);
    }
    m_param = t;
    Evaluate(m_segment, t, m_point);
    MakeModifiedNextPoint();
}

// Starts at the beginning of `path`; with `astar`, first walks there by A*.
void CAISplinePathTraversal::PrepareForTraversalForward(CAISplinePath* path, bool astar, CAIFilterObject* filter) {
    m_path = path;
    m_segmentIndex = 0;
    m_segment = &path->m_segments[m_segmentIndex];
    m_param = 0.0f;
    Evaluate(m_segment, 0.0f, m_point);
    if (astar) {
        if (filter->m_pAIObject->SetupAStarPathWalkForPatrol(*(CAIFilterRealVector3*)m_point)) {
            m_state = SPLINE_ASTAR;
        } else {
            m_state = SPLINE_WALK;
        }
    } else {
        m_state = SPLINE_WALK;
    }
    m_lastDistSq = 0.0f;
}

// Resumes after an interruption: by A* if more than 1.5 away from the curve.
void CAISplinePathTraversal::ContinueTraversalForwardAStar(CAIFilterObject* filter) {
    if (((CAIFilterRealPosition*)m_point)->GetDistanceXYReal(filter->m_pAIObject->m_position) > 1.5f) {
        if (filter->m_pAIObject->SetupAStarPathWalkForPatrol(*(CAIFilterRealVector3*)m_point)) {
            m_state = SPLINE_ASTAR;
        } else {
            m_state = SPLINE_WALK;
        }
        m_lastDistSq = 0.0f;
    }
}

// Whether the walker is close enough to the point to move it on (the
// distance has dropped below 2/3 of the last; otherwise remembers it).
static inline bool CloseEnough(CAISplinePathTraversal* self, const CAIFilterRealPosition& pos) {
    float d = ((CAIFilterRealPosition*)self->m_point)->GetDistanceSquaredXYReal(pos);
    if (2.0f * (self->m_lastDistSq - d) >= d) {
        return true;
    }
    self->m_lastDistSq = d;
    return false;
}

// Advances along the path from `pos` and gives the point to walk to (while
// still walking to the start by A*, updates that walk instead).
void CAISplinePathTraversal::GetNextPointForward(const CAIFilterRealPosition& pos, CAIFilterRealPosition* out,
                                                 CAIFilterObject* filter) {
    bool advance = true;
    switch (m_state) {
    case SPLINE_END:
    case SPLINE_ASTAR_END:
        Evaluate(&m_path->m_segments[m_path->m_lastSegment], 0.0f, m_point);
        if (CloseEnough(this, pos)) {
            if (filter != NULL) {
                BSObjectTriggerEvent(WalkerScript(filter), SPLINEEVT_DONE, NULL, NULL, false);
                filter->m_pAIObject->m_moveMode = AIMOVE_NONE;
            }
            m_state = SPLINE_DONE;
        }
        break;
    case SPLINE_WALK:
    case SPLINE_ASTAR:
        advance = CloseEnough(this, pos);
        // fall through
    default:
        if (advance) {
            float t = m_segment->GetClosestParameter(m_param, *(const CAIFilterRealVector3*)&pos);
            if (t >= 0.0f) {
                m_param = t;
            }
            if (m_param >= 1.0f) {
                if (m_segmentIndex == m_path->m_lastSegment - 1) {
                    m_lastDistSq = 0.0f;
                    if (m_state == SPLINE_ASTAR || m_state == SPLINE_ASTAR_ON) {
                        m_state = SPLINE_ASTAR_END;
                    } else {
                        m_state = SPLINE_END;
                    }
                    Evaluate(&m_path->m_segments[m_path->m_lastSegment], 0.0f, m_point);
                } else {
                    if (filter != NULL) {
                        BSObjectTriggerEvent(WalkerScript(filter), SPLINEEVT_NEXT_SEGMENT, NULL, NULL, false);
                    }
                    m_segmentIndex++;
                    m_segment++;
                    SetupNextSegmentForward(pos);
                }
            } else {
                if (m_state == SPLINE_WALK) {
                    if (filter != NULL) {
                        BSObjectTriggerEvent(WalkerScript(filter), SPLINEEVT_ON_PATH, NULL, NULL, false);
                    }
                    m_state = SPLINE_WALK_ON;
                } else if (m_state == SPLINE_ASTAR) {
                    if (filter != NULL) {
                        BSObjectTriggerEvent(WalkerScript(filter), SPLINEEVT_ON_PATH, NULL, NULL, false);
                    }
                    m_state = SPLINE_ASTAR_ON;
                }
                Evaluate(m_segment, m_param, m_point);
                MakeModifiedNextPoint();
            }
        }
        break;
    }
    if (m_state == SPLINE_ASTAR) {
        filter->m_pAIObject->UpdateAStarPathWalk();
        return;
    }
    out->field_0x0 = m_point[0];
    out->field_0x4 = m_point[1];
    out->field_0x8 = m_point[2];
}

void CAISplinePathTraversal::GetFinalPointForward(CAIFilterRealPosition* out) {
    Evaluate(&m_path->m_segments[m_path->m_lastSegment], 0.0f, &out->field_0x0);
}

CAISplinePathManager::CAISplinePathManager(unsigned long numPaths) {
    m_numPaths = numPaths * 2;
    m_paths = new CAISplinePath[m_numPaths];
    m_points = NULL;
    m_gamma = NULL;
    m_delta = NULL;
    m_tangents = NULL;
}

void CAISplinePathManager::AllocateGenerateBuffers() {
    m_points = (AISplineVec*)DWI_alloc("AI Spline Path Points", 0x4000, 0x100);
    m_gamma = (f32*)DWI_alloc("AI Spline Path Diagonals", 0x1000, 0x100);
    m_delta = (AISplineVec*)DWI_alloc("AI Spline Path Deltas", 0x4000, 0x100);
    m_tangents = (AISplineVec*)DWI_alloc("AI Spline Path Derivatives", 0x4000, 0x100);
}

void CAISplinePathManager::FreeGenerateBuffers() {
    MEM_free(m_points);
    MEM_free(m_gamma);
    MEM_free(m_delta);
    MEM_free(m_tangents);
    m_points = NULL;
    m_gamma = NULL;
    m_delta = NULL;
    m_tangents = NULL;
}

// Path `index` from the points, and its reverse (id + 0x1000000) in the
// second half of the table.
void CAISplinePathManager::GenerateSplinePath(unsigned long index, unsigned long id, unsigned long numPoints,
                                              const _aifilter_simple_v3* points) {
    m_paths[index].GenerateTestSplinePath(*this, numPoints, points, false);
    m_paths[index].m_id = id;
    m_paths[index + (m_numPaths >> 1)].GenerateTestSplinePath(*this, numPoints, points, true);
    m_paths[index + (m_numPaths >> 1)].m_id = id + 0x1000000;
}
