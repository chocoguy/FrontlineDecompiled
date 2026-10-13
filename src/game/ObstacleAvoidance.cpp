// CObstacleAvoidance: each update probes one of 13 lines fanned out in front
// of the AI (at three heights over three sweeps) against the environment.
// When the line to the objective is blocked, DetermineNewObjectivePosition
// steers to the nearest free line, or to a step point over a low obstacle.
#include "game/ObstacleAvoidance.h"
#include "game/AIObject.h"
#include "game/bsobject.h"
#include "game/vector.h"
#include "string.h"

#define OBSTACLE_NUM_ANGLES 11
#define OBSTACLE_BASE_RANGE 112.5f

// Line directions in (forward, right, up) units, scaled by m_scale. Lines 11
// and 12 are short probes beside the center line and count as line 5.
CVector3 f_rv3ObstacleAvoidanceLines[OBSTACLE_NUM_LINES];
// Yaw range (radians) covered by each of lines 0..10.
f32 f_rrObstacleAvoidanceLineAngleMin[OBSTACLE_NUM_ANGLES];
f32 f_rrObstacleAvoidanceLineAngleMax[OBSTACLE_NUM_ANGLES];
bool f_bObstacleAvoidanceLinesInitialized;

static inline void CopyPosition(CAIFilterRealPosition& dst, const CAIFilterRealPosition& src) {
    dst.field_0x0 = src.field_0x0;
    dst.field_0x4 = src.field_0x4;
    dst.field_0x8 = src.field_0x8;
}

// Steers toward m_target, or toward the nearest unblocked probe line.
void CObstacleAvoidance::DetermineNewObjectivePosition(CAIFilterRealPosition& out) {
    switch (m_state) {
    case OBSTACLE_STATE_NONE:
        break;
    case OBSTACLE_STATE_STEP:
        CopyPosition(out, m_target);
        if (m_pOwner->m_position.GetDistanceSquaredXYReal(m_stepPosition) < 0.25f + m_stepHeight) {
            m_state = OBSTACLE_STATE_STEP_DONE;
            BSObjectTriggerEvent(((CAIFilterSoldierObject*)m_pOwner->m_pFilter)->m_pBSObject,
                                 0x90, NULL, NULL, false);
        }
        return;
    default:
        CopyPosition(out, m_target);
        return;
    }

    CAIObject* owner = m_pOwner;
    float distSq = owner->m_position.GetDistanceSquaredXYReal(m_target);
    float yaw, pitch;
    ((CAIFilterRealEulerDirection*)&owner->field_0x60)
        ->CalculateAngleDiff(owner->m_position, m_target, &yaw, &pitch);

    int line;
    for (line = 0; line < OBSTACLE_NUM_ANGLES; line++) {
        if (yaw >= f_rrObstacleAvoidanceLineAngleMin[line] &&
            yaw <= f_rrObstacleAvoidanceLineAngleMax[line]) {
            break;
        }
    }
    if (line == OBSTACLE_NUM_ANGLES) {
        line = OBSTACLE_CENTER_LINE;
    }

    CObstacleGridEntry* entry = &m_grid[line];
    bool direct;
    if (distSq < entry->m_dist[1] && distSq < entry->m_dist[2]) {
        // the objective is closer than any obstacle on its line
        direct = true;
    } else {
        int mask = entry->GetBlockedMask();
        bool step;
        if (!(m_stepHeight != 0.0f)) {
            step = false;
        } else if (line != OBSTACLE_CENTER_LINE) {
            step = false;
        } else {
            switch (mask) {
            case 0:
            case 1:
            case 5:
            case 7:
                step = false;
                break;
            case 2:
            case 3:
            case 6:
                step = false;
                break;
            default:
                // only the lowest probe is blocked: step over it if it is far enough
                step = entry->m_dist[0] >= 0.25f + m_stepHeight;
                break;
            }
        }
        if (step) {
            m_state = OBSTACLE_STATE_STEP;
            direct = true;
            CopyPosition(m_stepPosition, entry->m_end);
        } else {
            direct = mask == 0;
        }
    }
    if (direct) {
        CopyPosition(out, m_target);
        m_side = 0;
        return;
    }

    int left = line;
    int right = line;
    int preferred = -1;
    if (m_side == 1) {
        if (line > 0) {
            left = preferred = line - 1;
        }
    } else if (m_side == 2 && line < 10) {
        right = preferred = line + 1;
    }
    if (preferred != -1 && m_grid[preferred].GetBlockedMask() == 0) {
        CopyPosition(out, m_grid[preferred].m_end);
        return;
    }

    do {
        if (left > 0) {
            left--;
            if (m_grid[left].GetBlockedMask() == 0) {
                CopyPosition(out, m_grid[left].m_end);
                return;
            }
        }
        if (right < 10) {
            right++;
            if (m_grid[right].GetBlockedMask() == 0) {
                CopyPosition(out, m_grid[right].m_end);
                return;
            }
        }
    } while (left != 0 || right != 10);
    CopyPosition(out, m_target);
}

// Probes line m_line at height m_height into m_work. After all 13 lines the
// range adapts to how much was hit, and after three heights m_work becomes
// m_grid.
void CObstacleAvoidance::RunEnvironmentCollisionChecks() {
    CAIObject* owner = m_pOwner;
    float range = m_range;
    float scale = m_scale;
    CAIFilterRealPosition start;
    CAIFilterRealPosition end;
    CAIFilterRealVector3 forward;
    CAIFilterRealVector3 right;
    CAIFilterRealVector3 up;

    CopyPosition(start, owner->m_position);
    start.field_0x8 += m_height;
    owner->GetCoordinateAxisDirection(&forward, &right, &up);

    // the side probes start half a unit behind or ahead
    if (m_line == 12) {
        CAIFilterRealVector3 offset;
        offset.field_0x0 = forward.field_0x0 * -0.5f;
        offset.field_0x4 = forward.field_0x4 * -0.5f;
        offset.field_0x8 = forward.field_0x8 * -0.5f;
        start.field_0x0 += offset.field_0x0;
        start.field_0x4 += offset.field_0x4;
        start.field_0x8 += offset.field_0x8;
    } else if (m_line == 11) {
        CAIFilterRealVector3 offset;
        offset.field_0x0 = forward.field_0x0 * 0.5f;
        offset.field_0x4 = forward.field_0x4 * 0.5f;
        offset.field_0x8 = forward.field_0x8 * 0.5f;
        start.field_0x0 += offset.field_0x0;
        start.field_0x4 += offset.field_0x4;
        start.field_0x8 += offset.field_0x8;
    }

    // end = start + forward * f + right * r + up * u (products rounded first)
    CVector3& dir = f_rv3ObstacleAvoidanceLines[m_line];
    float f = scale * dir.x;
    float r = scale * dir.y;
    float u = scale * dir.z;
    forward.field_0x0 *= f;
    forward.field_0x4 *= f;
    forward.field_0x8 *= f;
    right.field_0x0 *= r;
    right.field_0x4 *= r;
    right.field_0x8 *= r;
    up.field_0x0 *= u;
    up.field_0x4 *= u;
    up.field_0x8 *= u;
    end.field_0x0 = start.field_0x0 + forward.field_0x0;
    end.field_0x4 = start.field_0x4 + forward.field_0x4;
    end.field_0x8 = start.field_0x8 + forward.field_0x8;
    end.field_0x0 += right.field_0x0;
    end.field_0x4 += right.field_0x4;
    end.field_0x8 += right.field_0x8;
    end.field_0x0 += up.field_0x0;
    end.field_0x4 += up.field_0x4;
    end.field_0x8 += up.field_0x8;
    CopyPosition(m_work[m_line].m_end, end);

    CAIFilterObject* filter = (CAIFilterObject*)m_pOwner->m_pFilter;
    float maxDist;
    float dist;
    if (m_line == 11 || m_line == 12) {
        maxDist = 7.25f;
        dist = CAIFilterGlobal::GetCollisionDistLineToEnvironment(start, end, filter, 7.25f);
    } else {
        maxDist = start.GetDistanceSquaredXYZReal(end);
        dist = CAIFilterGlobal::GetCollisionDistLineToEnvironment(start, end, filter, maxDist);
    }

    if (dist == -1.0f) {
        if (m_line == 11 || m_line == 5 || m_line == 12) {
            m_centerHold[m_pass]--;
            if (m_centerHold[m_pass] <= 0) {
                m_work[OBSTACLE_CENTER_LINE].SetDist(m_pass, range, range);
            }
        } else {
            m_work[m_line].SetDist(m_pass, maxDist, maxDist);
        }
        m_balance--;
    } else {
        if (m_line == 5) {
            m_centerHold[m_pass] = 3;
            m_work[OBSTACLE_CENTER_LINE].SetDist(m_pass, dist, range);
        } else if (m_line == 11 || m_line == 12) {
            m_centerHold[m_pass] = 3;
            m_work[OBSTACLE_CENTER_LINE].SetDist(m_pass, 3.0f * dist, range);
        } else {
            m_work[m_line].SetDist(m_pass, dist, maxDist);
        }
        m_balance++;
    }

    switch (m_state) {
    case 0:
        break;
    case 1:
    case 2:
        // clearance just above the owner's feet
        CopyPosition(start, m_pOwner->m_position);
        start.field_0x8 -= 0.1f;
        CopyPosition(end, start);
        end.field_0x8 += 2.0f;
        dist = CAIFilterGlobal::GetCollisionDistLineToEnvironment(
            start, end, (CAIFilterObject*)m_pOwner->m_pFilter, 4.0f);
        if (dist == -1.0f) {
            m_upClearance = 4.0f;
        } else {
            m_upClearance = dist;
        }
        break;
    case OBSTACLE_STATE_STEP_DONE:
        if ((m_pOwner->m_flags & 4) == 4) {
            m_pOwner->m_flags &= ~4;
            m_state = OBSTACLE_STATE_NONE;
        } else {
            // ground below the step
            CopyPosition(start, m_pOwner->m_position);
            start.field_0x8 -= 2.1f;
            CopyPosition(end, start);
            end.field_0x8 -= 2.0f;
            dist = CAIFilterGlobal::GetCollisionDistLineToEnvironment(
                start, end, (CAIFilterObject*)m_pOwner->m_pFilter, 4.0f);
            if (dist == -1.0f) {
                m_downClearance = 4.0f;
            } else {
                m_downClearance = dist;
            }
        }
        break;
    }

    m_line++;
    if (m_line >= OBSTACLE_NUM_LINES) {
        m_line = 0;
        // mostly clear: probe further; mostly blocked: probe closer
        if (m_balance <= -26) {
            m_balance = 0;
            if (m_range < 149.99625f) {
                m_range += 18.750937f;
                m_scale = m_range / OBSTACLE_BASE_RANGE;
            }
        }
        if (m_balance >= 13) {
            m_balance = 0;
            if (m_range > 74.9925f) {
                m_range -= 18.750937f;
                m_scale = m_range / OBSTACLE_BASE_RANGE;
            }
        }
        switch (m_pass) {
        case 1:
            m_height = 1.2f;
            m_pass = 2;
            break;
        case 2:
            m_height = 0.4f;
            m_pass = 0;
            m_balance = 0;
            memcpy(m_grid, m_work, sizeof(m_grid));
            break;
        default:
            m_height = 0.8f;
            m_pass = 1;
            break;
        }
    }
}

void CObstacleAvoidance::PrepareForAvoidance(CAIFilterRealPosition& target) {
    m_line = 0;
    m_height = 0.4f;
    m_pass = 0;
    CopyPosition(m_target, target);
    m_state = OBSTACLE_STATE_NONE;
    m_side = 0;
    m_balance = 0;
    m_range = OBSTACLE_BASE_RANGE;
    m_scale = 1.0f;
    m_centerHold[0] = 0;
    m_centerHold[1] = 0;
    m_centerHold[2] = 0;
    for (CObstacleGridEntry* e = m_work; e < &m_work[OBSTACLE_NUM_LINES]; e++) {
        float range = m_range;
        e->SetDist(0, range, range);
        e->SetDist(1, range, range);
        e->SetDist(2, range, range);
        CopyPosition(e->m_end, m_target);
    }
    memcpy(m_grid, m_work, sizeof(m_grid));
    m_upClearance = 4.0f;
    m_downClearance = 4.0f;
    field_0x38C = 0.0f;
    field_0x390 = 0.0f;
    m_stepHeight = 0.0f;
}

CObstacleAvoidance::~CObstacleAvoidance() {}

CObstacleAvoidance::CObstacleAvoidance() {
    if (!f_bObstacleAvoidanceLinesInitialized) {
        CVector3* l = f_rv3ObstacleAvoidanceLines;
        l[0].x = -15.0f;    l[0].y = -5.625f;  l[0].z = 0.0f;
        l[1].x = -11.25f;   l[1].y = 0.0f;     l[1].z = 0.0f;
        l[2].x = -9.375f;   l[2].y = 4.5f;     l[2].z = 0.0f;
        l[3].x = -7.5f;     l[3].y = 7.5f;     l[3].z = 0.0f;
        l[4].x = -3.75f;    l[4].y = 7.5f;     l[4].z = 0.0f;
        l[5].x = 0.0f;      l[5].y = 7.5f;     l[5].z = 0.0f;
        l[6].x = 3.75f;     l[6].y = 7.5f;     l[6].z = 0.0f;
        l[7].x = 7.5f;      l[7].y = 7.5f;     l[7].z = 0.0f;
        l[8].x = 9.375f;    l[8].y = 4.5f;     l[8].z = 0.0f;
        l[9].x = 11.25f;    l[9].y = 0.0f;     l[9].z = 0.0f;
        l[10].x = 15.0f;    l[10].y = -5.625f; l[10].z = 0.0f;
        l[11].x = -1.0f;    l[11].y = 2.5f;    l[11].z = 0.0f;
        l[12].x = 1.0f;     l[12].y = 2.5f;    l[12].z = 0.0f;

        // sectors of pi/8 around each line, line 5 straight ahead
        f32* lo = f_rrObstacleAvoidanceLineAngleMin;
        f32* hi = f_rrObstacleAvoidanceLineAngleMax;
        lo[0] = 1.7671459f;    hi[0] = 3.1415927f;
        lo[1] = 1.3744467f;    hi[1] = 1.7671459f;
        lo[2] = 0.9817477f;    hi[2] = 1.3744467f;
        lo[3] = 0.5890486f;    hi[3] = 0.9817477f;
        lo[4] = 0.19634955f;   hi[4] = 0.5890486f;
        lo[5] = -0.19634955f;  hi[5] = 0.19634955f;
        lo[6] = -0.5890486f;   hi[6] = -0.19634955f;
        lo[7] = -0.9817477f;   hi[7] = -0.5890486f;
        lo[8] = -1.3744467f;   hi[8] = -0.9817477f;
        lo[9] = -1.7671459f;   hi[9] = -1.3744467f;
        lo[10] = -3.1415927f;  hi[10] = -1.7671459f;
        f_bObstacleAvoidanceLinesInitialized = true;
    }
}

CObstacleGridEntry::~CObstacleGridEntry() {}

CObstacleGridEntry::CObstacleGridEntry() {}
