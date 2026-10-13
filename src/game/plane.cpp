#include "game/plane.h"

#include <math.h>
#include "game/line.h"
#include "game/matrix.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

// Below this cosine (about 0.1 degree) the segment counts as parallel.
#define PARALLEL_EPSILON 0.001745328f

CPlane::CPlane(CVector3 normal, float d) {
    Normal().Copy(normal);
    m_d = d;
}

void CPlane::Transform(const CMatrix& m) {
    CVector3 point;
    point.x = m_d * m_normal[0];
    point.y = m_d * m_normal[1];
    point.z = m_d * m_normal[2];
    CVector3 out;
    m.TransformPoint(out, point);
    m.TransformVector(Normal(), Normal());
    m_d = m_normal[0] * out.x + m_normal[1] * out.y + m_normal[2] * out.z;
}

bool CPlane::GetIntersection(const CLine3& line, float& t, CVector3* point) const {
    CVector3 dir = line.m_dir;
    CVector3 start = line.m_start;
    float denom = m_normal[0] * dir.x + m_normal[1] * dir.y + m_normal[2] * dir.z;
    float dist = m_normal[0] * start.x + m_normal[1] * start.y + m_normal[2] * start.z;

    if ((float)fabs(denom) > PARALLEL_EPSILON * line.GetLength()) {
        t = (m_d - dist) / denom;
    } else if ((float)fabs(dist) > PARALLEL_EPSILON * start.Length()) {
        t = -1.0f;
    } else {
        t = 1.0f;
    }

    bool hit = false;
    if (t >= 0.0f && t <= 1.0f) {
        hit = true;
    }
    if (hit && point != NULL) {
        point->x = t * dir.x;
        point->y = t * dir.y;
        point->z = t * dir.z;
        point->x += start.x;
        point->y += start.y;
        point->z += start.z;
    }
    return hit;
}
