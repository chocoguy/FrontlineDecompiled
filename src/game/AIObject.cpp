// CAIObject: the AI side of soldiers, tanks and the player. Each AI object
// is on a global list; it tracks a target (filtering its position into an
// aim point with some error), checks vision (line of sight, field of view,
// "vision seeking" around a spot), walks A* paths over the level's path
// nodes or spline paths, takes cover points, and tells its script about
// what happens through events.
#include "game/AIObject.h"
#include "game/AIFilter.h"
#include "game/AISplinePath.h"
#include "game/AIStatus.h"
#include "game/BSGO_Basic.h"
#include "game/CoverPoint.h"
#include "game/ISceneNode.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/PathNode.h"
#include "game/SearchNode.h"
#include "game/SearchNodeList.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bstimer.h"
#include "game/bstypes.h"
#include "game/gcscratchpad.h"
#include "game/matrix.h"
#include "game/propdat.h"
#include "game/proxtrig.h"
#include "game/quaternion.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

// Events sent to the object's script.
#define AIEVT_AT_COVER 0x38          // reached the cover point (data: COVERPOINT_TYPE)
#define AIEVT_NEAR_COVER 0x39        // close to the cover point
#define AIEVT_NEAR_COVER_HIGH 0x3A   // same, for the second set of peek types
#define AIEVT_COVER_STEP 0x3B        // one step from the cover point
#define AIEVT_COVER_STEP_HIGH 0x3C
#define AIEVT_COVER_LOST 0x3E        // cover point taken or no longer useful
#define AIEVT_TARGET_LOST 0x43
#define AIEVT_FOLLOW_LOST 0x44
#define AIEVT_ARRIVED 0x98
#define AIEVT_PATH_FAILED 0xC1

#define MGPOINT_CRC 0x5B607813

int g_iAIObjectCount;
static CAIObject* g_paioAIObjectListStart;

static inline float AINow() {
    return g_aigAIFilterGlobalObject.m_time;
}

// The BSP leaf the object's game object is in.
static inline const _PropBSPLeaf* AILeaf(CAIFilterObject* filter) {
    BSGO_Basic* go = ((CAIFilterSoldierObject*)filter)->m_pGameObject;
    return ((_ProximityData*)go->GetProximityData())->m_newLeaf;
}

static inline void Normalize(CAIFilterRealVector3& v) {
    float len = v.field_0x8 * v.field_0x8 + (v.field_0x0 * v.field_0x0 + v.field_0x4 * v.field_0x4);
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    if (len != 0.0f) {
        float inv = 1.0f / len;
        v.field_0x0 *= inv;
        v.field_0x4 *= inv;
        v.field_0x8 *= inv;
    }
}

static inline void CopyPos(CAIFilterRealPosition& dst, const CAIFilterRealPosition& src) {
    dst.field_0x0 = src.field_0x0;
    dst.field_0x4 = src.field_0x4;
    dst.field_0x8 = src.field_0x8;
}

// Row 1 of a placed object's (or cover point's) orientation: the way it faces.
static inline void GetFacing(const f32* quat, CAIFilterRealVector3& dir) {
    CMatrix m;
    ((const CQuaternion*)quat)->GetMatrix(m);
    dir = *(CAIFilterRealVector3*)m.m[1];
}

// Vision ranges are shorter for some targets and for crouching ones.
static inline float TargetSightRange(CAIObject* target, float range) {
    if (target->m_field_0x730) {
        range *= 0.5f;
    }
    if (target->IsCrouching()) {
        range *= 0.75f;
    }
    return range;
}

static inline float CheckSightRange(float range, const char* warning) {
    if (range <= 0.001f) {
        printf(warning);
        range = 50.0f;
    }
    return range;
}

// Whether `pos` can be seen from `eye`: the line to one of two heights on
// the target (lower ones for a crouching target) must be clear.
static inline bool CanSeeFrom(const CAIFilterRealPosition& eye, const CAIFilterRealPosition& pos,
                              bool crouching, CAIFilterObject* filter) {
    float standing[2] = {0.8f, 1.2f};
    float crouched[2] = {0.8f, 0.1f};
    float* heights = standing;
    if (crouching) {
        heights = crouched;
    }
    for (int i = 0; i < 2; i++) {
        CAIFilterRealPosition end;
        CopyPos(end, pos);
        end.field_0x8 += *heights;
        if (!CAIFilterGlobal::CheckVisionLineToEnvironment(eye, end, filter)) {
            return true;
        }
        heights++;
    }
    return false;
}

CAIObjectParameters::~CAIObjectParameters() {}

CAIObject::CAIObject() {
    m_pNext = NULL;
    field_0xC = 0.0f;
    m_targetUpdateTime = 0.0f;
    m_nonAITarget = 0;
    m_pTarget = NULL;
    field_0x168 = 0.0f;
    field_0x16C = 0.0f;
    field_0x170 = 0.0f;
    field_0x178 = 0.0f;
    field_0x17C = 0.0f;
    field_0x180 = 0.0f;
    field_0x1A8 = 0;
    field_0x1AC = 0;
    m_flags = 0;
    m_numAttackers = 0;
    field_0x1F0 = 0.4f;
    field_0x1F4 = 0.08726646f;  // 5 degrees
    m_aimTime = 2.0f;
    m_inaccuracy = 2.0f;
    field_0x204 = 20.0f;
    m_sightRange = 30.0f;
    m_hearingRadius = 20.0f;
    m_moveMode = AIMOVE_NONE;
    m_nextWalkUpdate = 0.0f;
    m_coverFlags = 0;
    m_pCoverPoint = NULL;
    m_nextCoverCheckTime = 0.0f;
    m_avoidObstacles = false;
    m_pVisionCursor = NULL;
    m_pVisionObject = NULL;
    m_field_0x730 = false;
    m_avoidance.m_pOwner = this;
    m_targetSeen = 0;
}

CAIObject::~CAIObject() {
    SetTarget(NULL);
    RemoveFromGlobalList();
    CleanupAStarPathWalk();
}

void CAIObject::RemoveFromGlobalList() {
    CAIObject* prev = NULL;
    CAIObject* obj = g_paioAIObjectListStart;
    g_iAIObjectCount--;
    while (obj != NULL && obj != this) {
        prev = obj;
        obj = obj->m_pNext;
    }
    if (prev != NULL) {
        prev->m_pNext = m_pNext;
    } else {
        g_paioAIObjectListStart = m_pNext;
    }
    for (obj = g_paioAIObjectListStart; obj != NULL; obj = obj->m_pNext) {
        obj->RemovePointersTo(this);
    }
}

// Stops walking: back to the move point, the A* path freed.
static inline void StopPathWalk(CAIObject* self) {
    self->m_pCurNode = NULL;
    self->m_pGoalNode = NULL;
    self->m_pNextNode = NULL;
    CopyPos(self->m_destination, self->m_movePoint);
    self->m_lastDistSq = self->m_position.GetDistanceSquaredXYZReal(self->m_destination);
    self->m_approaching = 0;
    self->m_skippedNode = 0;
    self->m_moveMode = AIMOVE_NONE;
    self->CleanupAStarPathWalk();
}

// `obj` is going away.
void CAIObject::RemovePointersTo(CAIObject* obj) {
    if (m_pTarget == obj) {
        BSObjectTriggerEvent(GetScript(), AIEVT_TARGET_LOST, NULL, NULL, true);
        SetTarget(NULL);
    }
    if (m_pFollowObject == obj) {
        BSObjectTriggerEvent(GetScript(), AIEVT_FOLLOW_LOST, NULL, NULL, true);
        StopPathWalk(this);
        m_pFollowObject = NULL;
    }
    if (m_pVisionCursor == obj) {
        m_pVisionCursor = NULL;
    }
    if (m_pVisionObject == obj) {
        BSObjectTriggerEvent(GetScript(), AIEVT_TARGET_LOST, NULL, NULL, true);
        m_pVisionObject = NULL;
    }
}

CAIObject* CAIObject::GetGlobalListStart() {
    return g_paioAIObjectListStart;
}

void CAIObject::ResetGlobalList() {
    g_paioAIObjectListStart = NULL;
    g_iAIObjectCount = 0;
}

// Forgets having seen the target (the original's test is reversed: it
// clears the flag within 3 seconds of the last sighting).
bool CAIObject::UpdateReactionTime() {
    if (m_pTarget == NULL) {
        if (3.0f + m_lastSeenTime > AINow()) {
            m_targetSeen = 0;
        }
    } else if (m_targetSeen) {
        m_lastSeenTime = AINow();
    }
    return true;
}

void CAIObject::PreUpdate() {}

void CAIObject::Update(float) {}

void CAIObject::Init() {
    m_pNext = g_paioAIObjectListStart;
    g_paioAIObjectListStart = this;
    g_iAIObjectCount++;
}

// Whether a grenade thrown from here reaches the aim point: the launch
// speed for that distance (with some error), or -1 if the arc is blocked or
// the target is too close.
float CAIObject::TestGrenadeShot(float) {
    float dist = m_position.GetDistanceXYReal(m_targetPosition) +
                 3.464f * (m_targetPosition.field_0x8 - (0.5f + m_position.field_0x8));
    if (dist < 10.0f) {
        return -1.0f;
    }
    float reach = 0.95f * dist;
    float t = sqrtf(reach * 0.0062994817f);
    float half = 0.5f * t;
    CAIFilterRealPosition start;
    CopyPos(start, m_position);
    start.field_0x8 = m_position.field_0x8 + 0.5f;
    // The top of the arc, halfway along.
    CAIFilterRealVector3 apex;
    apex.field_0x0 = 0.0f;
    apex.field_0x4 = 0.5f * reach;
    apex.field_0x8 = 91.65313f * (half * half);
    apex.RotateAboutZ(m_yaw);
    apex.field_0x0 += start.field_0x0;
    apex.field_0x4 += start.field_0x4;
    apex.field_0x8 += start.field_0x8;
    if (CAIFilterGlobal::CheckCollisionLineToEnvironment(start, *(CAIFilterRealPosition*)&apex, m_pFilter)) {
        return -1.0f;
    }
    CopyPos(start, m_targetPosition);
    if (CAIFilterGlobal::CheckCollisionLineToEnvironment(*(CAIFilterRealPosition*)&apex, start, m_pFilter)) {
        return -1.0f;
    }
    return 600.0f * (t + MathFunRandomRealSigned(0.0f, 0.05f * (t * m_inaccuracy)));
}

// Steps the spring towards `target` (its velocity towards `targetVel`) over
// `dt` seconds, at most 0.1 s in steps of 1/120 s.
static inline void UpdateSpring(CAISpring& s, const CAIFilterRealVector3& target,
                                const CAIFilterRealVector3& targetVel, float dt) {
    float step = 0.008333334f;
    if (dt > 0.10000001f) {
        dt = 0.10000001f;
    }
    while (dt != 0.0f) {
        if (step > dt) {
            step = dt;
        }
        float half = 0.5f * step;
        dt -= step;
        float k = s.m_stiffness;
        float d = s.m_damping;
        CAIFilterRealVector3 v = s.m_vel;
        s.m_vel.field_0x0 += ((target.field_0x0 - s.m_pos.field_0x0) * k + (targetVel.field_0x0 - s.m_vel.field_0x0) * d) * step;
        s.m_vel.field_0x4 += ((target.field_0x4 - s.m_pos.field_0x4) * k + (targetVel.field_0x4 - s.m_vel.field_0x4) * d) * step;
        s.m_vel.field_0x8 += ((target.field_0x8 - s.m_pos.field_0x8) * k + (targetVel.field_0x8 - s.m_vel.field_0x8) * d) * step;
        v.field_0x0 += s.m_vel.field_0x0;
        v.field_0x4 += s.m_vel.field_0x4;
        v.field_0x8 += s.m_vel.field_0x8;
        v.field_0x0 *= half;
        v.field_0x4 *= half;
        v.field_0x8 *= half;
        s.m_pos.field_0x0 += v.field_0x0;
        s.m_pos.field_0x4 += v.field_0x4;
        s.m_pos.field_0x8 += v.field_0x8;
    }
}

// Raises the aim point from the target's feet, by m_aimHeightMode.
static inline void AdjustAimHeight(CAIObject* self) {
    CAIObject* target;
    switch (self->m_aimHeightMode) {
    default:
        self->m_targetPosition.field_0x8 += 1.0f;
        target = self->m_pTarget;
        if (target != NULL && target->IsCrouching()) {
            self->m_targetPosition.field_0x8 += -0.8f;
        }
        break;
    case 3:
        if (self->m_aimHeightChoice == 0) {
            if (MathFunTestPercent(MathFunGetRandomPercent(), 10)) {
                self->m_aimHeightChoice = 1;
            } else {
                self->m_aimHeightChoice = 2;
            }
        }
        if (self->m_aimHeightChoice != 2) {
            self->m_targetPosition.field_0x8 += 1.2f;
        } else {
            self->m_targetPosition.field_0x8 += 1.0f;
        }
        target = self->m_pTarget;
        if (target != NULL && target->IsCrouching()) {
            self->m_targetPosition.field_0x8 += -0.8f;
        }
        break;
    case 4:
        self->m_targetPosition.field_0x8 += 1.0f;
        target = self->m_pTarget;
        if (target != NULL && target->IsCrouching()) {
            self->m_targetPosition.field_0x8 += -0.8f;
        }
        break;
    case 5: {
        if (self->m_aimHeightChoice == 0) {
            s64 roll = MathFunGetRandomPercent();
            if (MathFunTestPercent(roll, 10)) {
                self->m_aimHeightChoice = 6;
            } else if (MathFunTestPercent(roll, 80)) {
                self->m_aimHeightChoice = 6;
            } else {
                self->m_aimHeightChoice = 8;
            }
        }
        switch (self->m_aimHeightChoice) {
        default:
            self->m_targetPosition.field_0x8 += 0.5f;
            target = self->m_pTarget;
            if (target != NULL && target->IsCrouching()) {
                self->m_targetPosition.field_0x8 += -0.8f;
            }
            break;
        case 7:
            self->m_targetPosition.field_0x8 += -0.5f;
            break;
        case 8:
            self->m_targetPosition.field_0x8 += 0.5f;
            self->m_targetPosition.field_0x0 += 1.25f;
            self->m_targetPosition.field_0x4 += 1.25f;
            break;
        }
        break;
    }
    case 6:
        break;
    case 7:
        self->m_targetPosition.field_0x8 += 1.2f;
        target = self->m_pTarget;
        if (target != NULL && target->IsCrouching()) {
            self->m_targetPosition.field_0x8 += 1.2f;
        }
        break;
    case 8:
        self->m_targetPosition.field_0x8 += 1.0f;
        target = self->m_pTarget;
        if (target != NULL && target->IsCrouching()) {
            self->m_targetPosition.field_0x8 += -0.8f;
        }
        break;
    }
}

// Filters the target's position (or where it was last seen) into the aim
// point, adds the aim error and the aim height.
void CAIObject::UpdateTargetPosition() {
    CAIObject* target = m_pTarget;
    CAIFilterRealPosition pos;
    if (m_targetVisible) {
        CopyPos(pos, target->m_position);
    } else {
        CopyPos(pos, m_lastSeenPosition);
    }
    CAIFilterRealVector3 relVel;
    relVel = target->m_velocity;
    float dt = AINow() - m_targetUpdateTime;
    relVel.field_0x0 -= m_velocity.field_0x0;
    relVel.field_0x4 -= m_velocity.field_0x4;
    relVel.field_0x8 -= m_velocity.field_0x8;
    CAIFilterRealVector3 zero;
    zero.field_0x0 = 0.0f;
    zero.field_0x4 = 0.0f;
    zero.field_0x8 = 0.0f;
    UpdateSpring(m_targetVelocity, relVel, zero, dt);
    UpdateSpring(m_targetTrack, *(CAIFilterRealVector3*)&pos, relVel, dt);

    float dist = m_position.GetDistanceXYZReal(pos);
    if ((m_flags & AIFLAG_AIM_DRIFT) == AIFLAG_AIM_DRIFT && m_targetVisible) {
        // The error drifts with the target's motion across the line of sight.
        CAIFilterRealVector3 dir;
        dir.field_0x0 = pos.field_0x0 - m_position.field_0x0;
        dir.field_0x4 = pos.field_0x4 - m_position.field_0x4;
        dir.field_0x8 = pos.field_0x8 - m_position.field_0x8;
        float scale = 0.1f * m_inaccuracy;
        CAIFilterRealVector3 drift;
        drift.field_0x0 = scale * m_targetVelocity.m_pos.field_0x0;
        drift.field_0x4 = scale * m_targetVelocity.m_pos.field_0x4;
        drift.field_0x8 = scale * m_targetVelocity.m_pos.field_0x8;
        Normalize(dir);
        float along = dir.field_0x8 * drift.field_0x8 + (dir.field_0x0 * drift.field_0x0 + dir.field_0x4 * drift.field_0x4);
        CAIFilterRealVector3 goal;
        goal.field_0x0 = drift.field_0x0 - dir.field_0x0 * along;
        goal.field_0x4 = drift.field_0x4 - dir.field_0x4 * along;
        goal.field_0x8 = drift.field_0x8 - dir.field_0x8 * along;
        UpdateSpring(m_aimError, goal, zero, dt);
    }
    if ((m_flags & AIFLAG_FIRED) == AIFLAG_FIRED) {
        m_flags &= ~AIFLAG_FIRED;
        m_flags &= ~AIFLAG_AIM_DRIFT;
    }
    m_targetPosition.field_0x0 = m_targetTrack.m_pos.field_0x0;
    m_targetPosition.field_0x4 = m_targetTrack.m_pos.field_0x4;
    m_targetPosition.field_0x8 = m_targetTrack.m_pos.field_0x8;
    AdjustAimHeight(this);
    m_targetPosition.field_0x0 += dist * m_aimError.m_pos.field_0x0;
    m_targetPosition.field_0x4 += dist * m_aimError.m_pos.field_0x4;
    m_targetPosition.field_0x8 += dist * m_aimError.m_pos.field_0x8;
    m_targetUpdateTime = AINow();
}

// Starts tracking a new target: the springs are tuned from m_aimTime and
// m_inaccuracy and start at the target, with a random aim error.
void CAIObject::CalculateTargetPosition() {
    m_aimHeightChoice = 0;
    float aimTime = m_aimTime;
    CAIFilterRealVector3 zero;
    zero.field_0x8 = 0.0f;
    float w = 6.2831855f / aimTime;
    zero.field_0x4 = 0.0f;
    zero.field_0x0 = 0.0f;
    CAIObject* target = m_pTarget;
    float w2 = w * w;
    float decay = 2.302585f / (3.0f * m_inaccuracy * aimTime);  // ln 10
    float k = decay * decay + w2;
    float d = 2.0f * decay;
    m_targetVelocity.m_stiffness = k;
    m_targetVelocity.m_damping = d;
    m_targetVelocity.m_pos = target->m_velocity;
    m_targetVelocity.m_vel = zero;

    CAIFilterRealVector3 err;
    err.field_0x0 = MathFunRandomRealSigned(m_inaccuracy, 2.0f * m_inaccuracy);
    err.field_0x4 = MathFunRandomRealSigned(m_inaccuracy, 2.0f * m_inaccuracy);
    err.field_0x8 = MathFunRandomRealSigned(m_inaccuracy, 2.0f * m_inaccuracy);
    m_aimError.m_stiffness = k;
    err.field_0x8 *= 0.05f;
    err.field_0x0 *= 0.05f;
    err.field_0x4 *= 0.05f;
    m_aimError.m_damping = d;
    m_aimError.m_pos = err;
    m_aimError.m_vel = zero;

    float decay2 = 2.302585f / m_aimTime;
    m_targetTrack.m_stiffness = decay2 * decay2 + w2;
    m_targetTrack.m_damping = 2.0f * decay2;
    *(CAIFilterRealPosition*)&m_targetTrack.m_pos = target->m_position;
    m_targetTrack.m_vel = zero;

    float dist = m_position.GetDistanceXYZReal(*(CAIFilterRealPosition*)&m_targetTrack.m_pos);
    m_targetPosition.field_0x0 = m_targetTrack.m_pos.field_0x0;
    m_targetPosition.field_0x4 = m_targetTrack.m_pos.field_0x4;
    m_targetPosition.field_0x8 = m_targetTrack.m_pos.field_0x8;
    AdjustAimHeight(this);
    err.field_0x0 = dist * m_aimError.m_pos.field_0x0;
    err.field_0x4 = dist * m_aimError.m_pos.field_0x4;
    err.field_0x8 = dist * m_aimError.m_pos.field_0x8;
    m_targetPosition.field_0x0 += err.field_0x0;
    m_targetPosition.field_0x4 += err.field_0x4;
    m_targetPosition.field_0x8 += err.field_0x8;
    m_targetUpdateTime = AINow();
}

// How far the environment is clear in a direction (relative to the axes)
// at chest and knee height, or 0 if there is a drop at the end, or -1 if
// all is clear.
float CAIObject::PerformCollisionTest(ECollisionTestDirection dir, float range) {
    CAIFilterRealPosition end;
    switch (dir) {
    case 0:
        *(CAIFilterRealVector3*)&end = m_side;
        end.field_0x0 *= range;
        end.field_0x4 *= range;
        end.field_0x8 *= range;
        break;
    case 1: {
        float s = 0.707f * range;
        end.field_0x0 = m_side.field_0x0 + m_forward.field_0x0;
        end.field_0x4 = m_side.field_0x4 + m_forward.field_0x4;
        end.field_0x8 = m_side.field_0x8 + m_forward.field_0x8;
        end.field_0x0 *= s;
        end.field_0x4 *= s;
        end.field_0x8 *= s;
        break;
    }
    case 2:
        *(CAIFilterRealVector3*)&end = m_forward;
        end.field_0x0 *= range;
        end.field_0x4 *= range;
        end.field_0x8 *= range;
        break;
    case 3: {
        float s = 0.707f * range;
        end.field_0x0 = m_forward.field_0x0 - m_side.field_0x0;
        end.field_0x4 = m_forward.field_0x4 - m_side.field_0x4;
        end.field_0x8 = m_forward.field_0x8 - m_side.field_0x8;
        end.field_0x0 *= s;
        end.field_0x4 *= s;
        end.field_0x8 *= s;
        break;
    }
    case 4: {
        float s = -range;
        *(CAIFilterRealVector3*)&end = m_side;
        end.field_0x0 *= s;
        end.field_0x4 *= s;
        end.field_0x8 *= s;
        break;
    }
    case 5: {
        float s = 0.707f * -range;
        end.field_0x0 = m_side.field_0x0 + m_forward.field_0x0;
        end.field_0x4 = m_side.field_0x4 + m_forward.field_0x4;
        end.field_0x8 = m_side.field_0x8 + m_forward.field_0x8;
        end.field_0x0 *= s;
        end.field_0x4 *= s;
        end.field_0x8 *= s;
        break;
    }
    case 6: {
        float s = -range;
        *(CAIFilterRealVector3*)&end = m_forward;
        end.field_0x0 *= s;
        end.field_0x4 *= s;
        end.field_0x8 *= s;
        break;
    }
    case 7: {
        float s = 0.707f * -range;
        end.field_0x0 = m_forward.field_0x0 - m_side.field_0x0;
        end.field_0x4 = m_forward.field_0x4 - m_side.field_0x4;
        end.field_0x8 = m_forward.field_0x8 - m_side.field_0x8;
        end.field_0x0 *= s;
        end.field_0x4 *= s;
        end.field_0x8 *= s;
        break;
    }
    }
    end.field_0x0 += m_position.field_0x0;
    end.field_0x4 += m_position.field_0x4;
    end.field_0x8 += m_position.field_0x8;
    CAIFilterRealPosition start;
    CopyPos(start, m_position);
    end.field_0x8 += 1.2f;
    start.field_0x8 += 1.2f;
    float dist = CAIFilterGlobal::GetCollisionDistLineToEnvironment(start, end, m_pFilter, range);
    if (dist != -1.0f) {
        return dist;
    }
    end.field_0x8 += -0.8000001f;
    start.field_0x8 += -0.8000001f;
    // (The original passes the previous result here and -0.8 below.)
    dist = CAIFilterGlobal::GetCollisionDistLineToEnvironment(start, end, m_pFilter, dist);
    if (dist != -1.0f) {
        return dist;
    }
    CopyPos(start, end);
    end.field_0x8 += -0.8f;
    if (CAIFilterGlobal::GetCollisionDistLineToEnvironment(start, end, m_pFilter, -0.8f) == -1.0f) {
        return 0.0f;
    }
    return -1.0f;
}

void CAIObject::SetLocation(float x, float y, float z) {
    m_position.field_0x0 = x;
    m_position.field_0x4 = y;
    m_position.field_0x8 = z;
}

void CAIObject::SetLocation(CAIFilterRealPosition& pos) {
    m_position.field_0x0 = pos.field_0x0;
    m_position.field_0x4 = pos.field_0x4;
    m_position.field_0x8 = pos.field_0x8;
}

// Sets the axes and derives the yaw (of forward) and the pitch of the side
// axis once the yaw is taken out.
void CAIObject::SetCoordinateAxisDirection(float fx, float fy, float fz, float sx, float sy, float sz,
                                           float ux, float uy, float uz) {
    m_forward.field_0x0 = fx;
    m_forward.field_0x4 = fy;
    m_forward.field_0x8 = fz;
    m_side.field_0x0 = sx;
    m_side.field_0x4 = sy;
    m_side.field_0x8 = sz;
    m_up.field_0x0 = ux;
    m_up.field_0x4 = uy;
    m_up.field_0x8 = uz;
    m_yaw = MathFunAtan2F(-m_forward.field_0x4, m_forward.field_0x0);
    CAIFilterRealVector3 side = m_side;
    side.RotateAboutZ(-m_yaw);
    field_0x64 = 0.0f;
    field_0x60 = MathFunAtan2F(-side.field_0x8, side.field_0x4);
}

void CAIObject::SetCoordinateAxisDirection(CAIFilterRealVector3& forward, CAIFilterRealVector3& side,
                                           CAIFilterRealVector3& up) {
    m_forward = forward;
    m_side = side;
    m_up = up;
    m_yaw = MathFunAtan2F(-m_forward.field_0x4, m_forward.field_0x0);
    CAIFilterRealVector3 s = m_side;
    s.RotateAboutZ(-m_yaw);
    field_0x64 = 0.0f;
    field_0x60 = MathFunAtan2F(-s.field_0x8, s.field_0x4);
}

void CAIObject::GetCoordinateAxisDirection(CAIFilterRealVector3* forward, CAIFilterRealVector3* side,
                                           CAIFilterRealVector3* up) {
    *forward = m_forward;
    *side = m_side;
    *up = m_up;
}

float CAIObject::GetDistanceXYZReal(CAIObject* obj) {
    return m_position.GetDistanceXYZReal(obj->m_position);
}

float CAIObject::GetDistanceSquaredXYZReal(CAIObject* obj) {
    return m_position.GetDistanceSquaredXYZReal(obj->m_position);
}

float CAIObject::GetDistanceSquaredXYZReal() {
    return m_position.GetDistanceSquaredXYZReal(m_targetPosition);
}

float CAIObject::GetDistanceSquaredXYReal(CAIObject* obj) {
    return m_position.GetDistanceSquaredXYReal(obj->m_position);
}

static inline void SetAttackerCount(CAIObject* obj, int count) {
    obj->m_numAttackers = count;
    if (obj->m_numAttackers <= 15) {
        obj->m_status.m_vars[AISTATUS_ATTACKERS] = obj->m_numAttackers;
    } else {
        obj->m_status.m_vars[AISTATUS_ATTACKERS] = 15;
    }
}

CAIObject* CAIObject::SetTarget(CAIObject* target) {
    m_nonAITarget = 0;
    if (target == m_pTarget) {
        return m_pTarget;
    }
    m_targetVisible = 0;
    CAIObject* old = m_pTarget;
    m_pTarget = target;
    if (target != NULL) {
        CopyPos(m_lastSeenPosition, target->m_position);
        m_targetVisible = 1;
        m_targetSeen = 1;
        CalculateTargetPosition();
    } else if ((m_moveMode == AIMOVE_ARBITRARY || m_moveMode == AIMOVE_ASTAR) &&
               (m_movePointType == 1 || m_movePointType == 2)) {
        // Was walking to the target.
        m_movePointType = AIMOVEPOINT_CANCELLED;
    }
    if (target != NULL) {
        SetAttackerCount(target, target->m_numAttackers + 1);
    }
    if (old != NULL) {
        SetAttackerCount(old, old->m_numAttackers - 1);
    }
    return old;
}

// Targets a script object's position (fixed) instead of an AI object.
void CAIObject::SetNonAITarget(BSObject* script) {
    SetTarget(NULL);
    m_nonAITarget = 1;
    if (script != NULL && script->m_trigger != NULL) {
        TriggerObjectData_struct* data = script->m_trigger->m_data;
        if (data != NULL) {
            m_targetTrack.m_pos.field_0x0 = data->m_position[0];
            m_targetTrack.m_pos.field_0x4 = data->m_position[1];
            m_targetTrack.m_pos.field_0x8 = data->m_position[2];
            m_targetPosition.field_0x0 = m_targetTrack.m_pos.field_0x0;
            m_targetPosition.field_0x4 = m_targetTrack.m_pos.field_0x4;
            m_targetPosition.field_0x8 = m_targetTrack.m_pos.field_0x8;
            m_lastSeenPosition.field_0x0 = m_targetTrack.m_pos.field_0x0;
            m_lastSeenPosition.field_0x4 = m_targetTrack.m_pos.field_0x4;
            m_lastSeenPosition.field_0x8 = m_targetTrack.m_pos.field_0x8;
            m_lastSeenPosition.field_0x8 -= 1.0f;
        }
    }
}

int CAIObject::ComputeStatusVisibility(CAIObject*) {
    return 0;
}

int CAIObject::ComputeStatusDistance(CAIObject* obj) {
    float d = 0.1f * m_position.GetDistanceXYReal(obj->m_position);
    if (d >= 15.0f) {
        return 15;
    }
    return (int)d;
}

// 0..15 by distance, finer when close: the distance compared with squares.
int CAIObject::ComputeStatusExponentialDistance(CAIObject* obj) {
    float d = m_position.GetDistanceXYZReal(obj->m_position);
    if (d < 64.0f) {
        if (d < 16.0f) {
            if (d < 4.0f) {
                return !(d < 1.0f);
            }
            if (d < 9.0f) {
                return 2;
            }
            return 3;
        }
        if (d < 36.0f) {
            if (d < 25.0f) {
                return 4;
            }
            return 5;
        }
        if (d < 49.0f) {
            return 6;
        }
        return 7;
    }
    if (d < 144.0f) {
        if (d < 100.0f) {
            if (d < 81.0f) {
                return 8;
            }
            return 9;
        }
        if (d < 121.0f) {
            return 10;
        }
        return 11;
    }
    if (d < 196.0f) {
        if (d < 169.0f) {
            return 12;
        }
        return 13;
    }
    if (d < 225.0f) {
        return 14;
    }
    return 15;
}

static inline int ThreatValue(CAIObject* self, CAIObject* obj, int threat, int attackers) {
    int value = (threat + (15 - self->ComputeStatusExponentialDistance(obj)) * 2) / (attackers + 3);
    if (value < 0) {
        return 0;
    }
    if (value > 15) {
        value = 15;
    }
    return value;
}

int CAIObject::ComputeStatusThreatValue(CAIObject* obj) {
    return ThreatValue(this, obj, obj->m_status.m_vars[AISTATUS_THREAT],
                       obj->m_status.m_vars[AISTATUS_ATTACKERS]);
}

int CAIObject::ComputeStatusLOSValue(CAIObject* obj) {
    float range = TargetSightRange(obj, m_sightRange);
    return IsObjectInLineOfSight(obj->m_position, range, obj->IsCrouching()) ? 15 : 0;
}

int CAIObject::ComputeStatusIsEnemy(CAIObject* obj) {
    return AIIsEnemyTeam(field_0x14, obj->field_0x14) ? 15 : 0;
}

void CAIObject::StartSplinePathWalkForward(CAISplinePath* path) {
    m_moveMode = AIMOVE_NONE;
    StopPathWalk(this);
    m_splinePath.PrepareForTraversalForward(path, true, m_pFilter);
    m_moveMode = AIMOVE_SPLINE;
    m_nextWalkUpdate = 0.1f + AINow();
    m_splinePath.GetNextPointForward(m_position, &m_destination, m_pFilter);
}

void CAIObject::WalkNextSplinePathPoint() {
    m_nextWalkUpdate = 0.1f + AINow();
    m_splinePath.GetNextPointForward(m_position, &m_destination, m_pFilter);
}

// Turns obstacle avoidance on or off; `stepHeight` is how high an obstacle
// may be to step over it.
void CAIObject::EngageObstacleAvoidance(bool on, float, float, float stepHeight) {
    m_avoidObstacles = on;
    if (m_avoidObstacles) {
        m_avoidance.PrepareForAvoidance(m_destination);
        m_avoidance.m_stepHeight = stepHeight * stepHeight;
    }
}

// A* path walk following another AI object.
bool CAIObject::StartAStarPathWalk(CAIObject* obj) {
    m_moveMode = AIMOVE_NONE;
    StopPathWalk(this);
    m_pFollowObject = NULL;
    CAIFilterRealPosition start;
    start = m_position;
    start.field_0x8 += 0.4f;
    CAIFilterRealPosition end;
    end = obj->m_position;
    CAIFilterRealVector3 startDir;
    startDir.field_0x8 = 0.0f;
    end.field_0x8 += 0.4f;
    startDir.field_0x4 = 0.0f;
    startDir.field_0x0 = 0.0f;
    m_pCurNode = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(start, startDir);
    if (m_pCurNode == NULL) {
        DebugMsg("StartAStarPathWalk: Could not find closest path node to start pos (%f,%f,%f), start dir (%f,%f,%f)\n",
                 start.field_0x0, start.field_0x4, start.field_0x8, startDir.field_0x0, startDir.field_0x4,
                 startDir.field_0x8);
        return false;
    }
    CAIFilterRealVector3 endDir;
    endDir.field_0x0 = start.field_0x0 - end.field_0x0;
    endDir.field_0x4 = start.field_0x4 - end.field_0x4;
    endDir.field_0x8 = start.field_0x8 - end.field_0x8;
    Normalize(endDir);
    CPathNode* goal = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(end, endDir);
    if (goal == NULL) {
        DebugMsg("StartAStarPathWalk: Could not find closest path node to end pos (%f,%f,%f), end dir (%f,%f,%f)\n",
                 end.field_0x0, end.field_0x4, end.field_0x8, endDir.field_0x0, endDir.field_0x4,
                 endDir.field_0x8);
        return false;
    }
    m_pGoalNode = NULL;
    GetNextPathNode(m_pCurNode, goal);
    if (m_pNextNode == NULL) {
        CPathNode* cur = m_pCurNode;
        DebugMsg("StartAStartPathWalk: Could not compute AStar path from node (%f,%f,%f), to node (%f,%f,%f)\n",
                 cur->m_pos[0], cur->m_pos[1], cur->m_pos[2], goal->m_pos[0], goal->m_pos[1], goal->m_pos[2]);
        return false;
    }
    m_moveMode = AIMOVE_ASTAR;
    m_pFollowObject = obj;
    CopyPos(m_pathGoal, obj->m_position);
    m_pathGoal.field_0x8 += 0.4f;
    CAIFilterRealPosition next;
    if (m_pNextNode != NULL) {
        next.field_0x0 = m_pNextNode->m_pos[0];
        next.field_0x4 = m_pNextNode->m_pos[1];
        next.field_0x8 = m_pNextNode->m_pos[2];
    } else {
        CopyPos(next, m_pathGoal);
    }
    m_lastDistSq = m_position.GetDistanceSquaredXYReal(next);
    m_approaching = 0;
    m_skippedNode = 0;
    EngageObstacleAvoidance(false, 0.0f, 0.0f, 0.0f);
    return true;
}

// Plans an A* path from here to m_movePoint (directions are the preferred
// ways to leave and arrive); `commit` also starts walking it.
bool CAIObject::SetupAStarPathWalk(const CAIFilterRealVector3& startDir, const CAIFilterRealVector3& endDir,
                                   bool commit) {
    CAIFilterRealPosition start;
    CopyPos(start, m_position);
    start.field_0x8 += 0.4f;
    CAIFilterRealPosition end;
    CopyPos(end, m_movePoint);
    end.field_0x8 += 0.4f;
    m_pFollowObject = NULL;
    CopyPos(m_pathGoal, m_movePoint);
    m_pathGoal.field_0x8 += 0.4f;
    m_pCurNode = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(start, startDir);
    if (m_pCurNode == NULL) {
        DebugMsg("SetupAStarPathWalk: Could not find closest path node to start pos (%f,%f,%f), start dir (%f,%f,%f)\n",
                 start.field_0x0, start.field_0x4, start.field_0x8, startDir.field_0x0, startDir.field_0x4,
                 startDir.field_0x8);
        return false;
    }
    CAIFilterRealVector3 dir = endDir;
    if (m_movePointType == AIMOVEPOINT_3D) {
        dir.field_0x8 = 0.0f;
        dir.field_0x4 = 0.0f;
        dir.field_0x0 = 0.0f;
    }
    CPathNode* goal = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(end, dir);
    if (goal == NULL) {
        DebugMsg("SetupAStarPathWalk: Could not find closest path node to end pos (%f,%f,%f), end dir (%f,%f,%f)\n",
                 end.field_0x0, end.field_0x4, end.field_0x8, endDir.field_0x0, endDir.field_0x4,
                 endDir.field_0x8);
        return false;
    }
    m_pGoalNode = NULL;
    GetNextPathNode(m_pCurNode, goal);
    if (m_pNextNode == NULL) {
        CPathNode* cur = m_pCurNode;
        DebugMsg("SetupAStartPathWalk: Could not compute AStar path from node (%f,%f,%f), to node (%f,%f,%f)\n",
                 cur->m_pos[0], cur->m_pos[1], cur->m_pos[2], goal->m_pos[0], goal->m_pos[1], goal->m_pos[2]);
        return false;
    }
    if (m_pNextNode != NULL) {
        m_pathDone = 0;
        m_destination.field_0x0 = m_pNextNode->m_pos[0];
        m_destination.field_0x4 = m_pNextNode->m_pos[1];
        m_destination.field_0x8 = m_pNextNode->m_pos[2];
    } else {
        m_pathDone = 1;
        CopyPos(m_destination, m_pathGoal);
    }
    if (commit) {
        m_lastDistSq = m_position.GetDistanceSquaredXYReal(m_destination);
        m_approaching = 0;
        m_skippedNode = 0;
        m_nextRepathTime = 1.0f + AINow();
        m_nextWalkUpdate = 0.3f + AINow();
        EngageObstacleAvoidance(false, 0.0f, 0.0f, 0.0f);
    }
    return true;
}

// Move points of types other than 3..10 follow the target (or where it was
// last seen), and are copied back to the script's vector.
static inline void UpdateMovePointFromTarget(CAIObject* self) {
    int type = self->m_movePointType;
    if (type < 3 || type >= 11) {
        CAIObject* target = self->m_pTarget;
        if (target != NULL && self->m_targetVisible) {
            CopyPos(self->m_movePoint, target->m_position);
        } else {
            CopyPos(self->m_movePoint, self->m_lastSeenPosition);
        }
        BS_STRUCT_Vector_struct* var = self->m_pMovePointVar;
        ((f32*)var)[0] = self->m_movePoint.field_0x0;
        ((f32*)var)[1] = self->m_movePoint.field_0x4;
        ((f32*)var)[2] = self->m_movePoint.field_0x8;
    }
}

// Plans and starts a patrol walk to `point`, arriving facing away
// from where we are now.
bool CAIObject::SetupAStarPathWalkForPatrol(const CAIFilterRealVector3& point) {
    StopPathWalk(this);
    m_movePointType = AIMOVEPOINT_PATROL;
    *(CAIFilterRealVector3*)&m_movePoint = point;
    UpdateMovePointFromTarget(this);
    CAIFilterRealVector3 dir;
    dir.field_0x0 = m_movePoint.field_0x0 - m_position.field_0x0;
    dir.field_0x4 = m_movePoint.field_0x4 - m_position.field_0x4;
    dir.field_0x8 = m_movePoint.field_0x8 - m_position.field_0x8;
    Normalize(dir);
    CAIFilterRealVector3 back;
    back.field_0x0 = -1.0f * dir.field_0x0;
    back.field_0x4 = -1.0f * dir.field_0x4;
    back.field_0x8 = -1.0f * dir.field_0x8;
    if (!SetupAStarPathWalk(dir, back, true)) {
        return false;
    }
    return true;
}

// A* walk to m_movePoint (or its script vector).
bool CAIObject::StartAStarPathWalk() {
    m_moveMode = AIMOVE_NONE;
    StopPathWalk(this);
    if (m_pMovePointVar != NULL) {
        m_movePoint.field_0x0 = ((f32*)m_pMovePointVar)[0];
        m_movePoint.field_0x4 = ((f32*)m_pMovePointVar)[1];
        m_movePoint.field_0x8 = ((f32*)m_pMovePointVar)[2];
    }
    UpdateMovePointFromTarget(this);
    CAIFilterRealVector3 startDir;
    startDir.field_0x8 = 0.0f;
    startDir.field_0x4 = 0.0f;
    startDir.field_0x0 = 0.0f;
    CAIFilterRealVector3 endDir;
    endDir.field_0x0 = m_position.field_0x0 - m_movePoint.field_0x0;
    endDir.field_0x4 = m_position.field_0x4 - m_movePoint.field_0x4;
    endDir.field_0x8 = m_position.field_0x8 - m_movePoint.field_0x8;
    Normalize(endDir);
    if (!SetupAStarPathWalk(startDir, endDir, true)) {
        m_moveMode = AIMOVE_NONE;
        return false;
    }
    m_moveMode = AIMOVE_ASTAR;
    return true;
}

// Heads for the next path node, then (by m_destination) the path goal.
static inline void SetDestinationToNextNode(CAIObject* self) {
    CPathNode* next = self->m_pNextNode;
    if (next != NULL) {
        self->m_destination.field_0x0 = next->m_pos[0];
        self->m_destination.field_0x4 = next->m_pos[1];
        self->m_destination.field_0x8 = next->m_pos[2];
    } else {
        CopyPos(self->m_destination, self->m_pathGoal);
        BSObjectTriggerEvent(self->GetScript(), AIEVT_PATH_FAILED, NULL, NULL, true);
    }
}

// Advances along the A* path: refreshes the goal (a followed object or the
// move point), replans when the goal's closest node changes, and moves to
// the next node once this one is reached or passed.
void CAIObject::UpdateAStarPathWalk() {
    CAIFilterRealPosition pos;
    CopyPos(pos, m_position);
    pos.field_0x8 += 0.4f;
    if (m_pFollowObject != NULL) {
        CopyPos(m_pathGoal, m_pFollowObject->m_position);
        m_pathGoal.field_0x8 += 0.4f;
    } else if (AINow() > m_nextWalkUpdate) {
        UpdateMovePointFromTarget(this);
        CopyPos(m_pathGoal, m_movePoint);
        m_pathGoal.field_0x8 += 0.4f;
    }
    CAIFilterRealVector3 goalDir;
    goalDir.field_0x0 = pos.field_0x0 - m_pathGoal.field_0x0;
    goalDir.field_0x4 = pos.field_0x4 - m_pathGoal.field_0x4;
    goalDir.field_0x8 = pos.field_0x8 - m_pathGoal.field_0x8;
    Normalize(goalDir);

    if (m_pCurNode != NULL && m_pGoalNode != NULL && !m_pathDone) {
        if (!(AINow() < m_nextWalkUpdate)) {
            CPathNode* goal = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(m_pathGoal, goalDir);
            if (goal == NULL) {
                goal = m_pGoalNode;
            }
            if (goal != m_pGoalNode && m_pCurNode != goal) {
                GetNextPathNode(m_pCurNode, goal);
            } else if (m_pCurNode == goal) {
                m_pGoalNode = goal;
            }
            m_nextWalkUpdate = 0.3f + AINow();
        }
        if (m_pCurNode == m_pGoalNode) {
            CopyPos(m_destination, m_pathGoal);
            return;
        }
        if (m_pNextNode != NULL && !m_pNextNode->m_enabled) {
            GetNextPathNode(m_pCurNode, NULL);
        }
        if (m_pNextNode != NULL) {
            CPathNode* cur = m_pCurNode;
            CAIFilterRealPosition from;
            from.field_0x0 = cur->m_pos[0];
            from.field_0x4 = cur->m_pos[1];
            from.field_0x8 = cur->m_pos[2];
            m_destination.field_0x0 = m_pNextNode->m_pos[0];
            m_destination.field_0x4 = m_pNextNode->m_pos[1];
            m_destination.field_0x8 = m_pNextNode->m_pos[2];
            float segX = m_destination.field_0x0 - from.field_0x0;
            float segY = m_destination.field_0x4 - from.field_0x4;
            float segZ = m_destination.field_0x8 - from.field_0x8;
            float offX = m_position.field_0x0 - m_destination.field_0x0;
            float offY = m_position.field_0x4 - m_destination.field_0x4;
            float offZ = m_position.field_0x8 - m_destination.field_0x8;
            float distSq = m_position.GetDistanceSquaredXYReal(m_destination);
            if (distSq <= 0.25f || (distSq < 4.0f && offZ * segZ + (offX * segX + offY * segY) > 0.0f)) {
                // Reached (or went past) the node.
                m_pCurNode = m_pNextNode;
                CPathNode* goal = m_pGoalNode;
                bool atGoal = m_pNextNode == goal;
                GetNextPathNode(m_pCurNode, goal);
                if (m_pNextNode != NULL) {
                    m_destination.field_0x0 = m_pNextNode->m_pos[0];
                    m_destination.field_0x4 = m_pNextNode->m_pos[1];
                    m_destination.field_0x8 = m_pNextNode->m_pos[2];
                } else {
                    m_pathDone = 1;
                    CopyPos(m_destination, m_pathGoal);
                    if (!atGoal) {
                        BSObjectTriggerEvent(GetScript(), AIEVT_PATH_FAILED, NULL, NULL, true);
                    }
                }
                m_lastDistSq = m_position.GetDistanceSquaredXYReal(m_destination);
                m_approaching = 0;
                m_skippedNode = 0;
                return;
            }
            if (m_approaching) {
                if (distSq > m_lastDistSq) {
                    // Moving away from the node: skip it, once.
                    if (!m_skippedNode) {
                        m_pCurNode = m_pNextNode;
                        GetNextPathNode(m_pCurNode, m_pGoalNode);
                        SetDestinationToNextNode(this);
                        m_lastDistSq = m_position.GetDistanceSquaredXYReal(m_destination);
                        m_approaching = 0;
                        m_skippedNode = 1;
                    } else {
                        CopyPos(m_destination, m_pathGoal);
                        BSObjectTriggerEvent(GetScript(), AIEVT_PATH_FAILED, NULL, NULL, true);
                    }
                }
            } else if (distSq < m_lastDistSq) {
                m_approaching = 1;
            }
            m_lastDistSq = distSq;
            return;
        }
        if (AINow() >= m_nextRepathTime) {
            CPathNode* goal = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(m_pathGoal, goalDir);
            CAIFilterRealVector3 none;
            none.field_0x8 = 0.0f;
            none.field_0x4 = 0.0f;
            none.field_0x0 = 0.0f;
            m_pCurNode = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(pos, none);
            if (m_pCurNode == NULL || goal == NULL) {
                CopyPos(m_destination, m_pathGoal);
                BSObjectTriggerEvent(GetScript(), AIEVT_PATH_FAILED, NULL, NULL, true);
            } else {
                m_pGoalNode = NULL;
                GetNextPathNode(m_pCurNode, goal);
                SetDestinationToNextNode(this);
                m_lastDistSq = m_position.GetDistanceSquaredXYReal(m_destination);
                m_approaching = 0;
                m_skippedNode = 0;
            }
            m_nextRepathTime = 1.0f + AINow();
        }
    } else if (!m_pathDone && AINow() >= m_nextRepathTime) {
        CPathNode* goal = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(m_pathGoal, goalDir);
        CAIFilterRealVector3 none;
        none.field_0x8 = 0.0f;
        none.field_0x4 = 0.0f;
        none.field_0x0 = 0.0f;
        m_pCurNode = g_aigAIFilterGlobalObject.GetClosestAStarPathNode(pos, none);
        if (m_pCurNode == NULL || goal == NULL) {
            CopyPos(m_destination, m_pathGoal);
        } else {
            m_pGoalNode = NULL;
            GetNextPathNode(m_pCurNode, goal);
            if (m_pNextNode != NULL) {
                m_destination.field_0x0 = m_pNextNode->m_pos[0];
                m_destination.field_0x4 = m_pNextNode->m_pos[1];
                m_destination.field_0x8 = m_pNextNode->m_pos[2];
            } else {
                CopyPos(m_destination, m_pathGoal);
                m_pCurNode = NULL;
                m_pGoalNode = NULL;
            }
            m_lastDistSq = m_position.GetDistanceSquaredXYReal(m_destination);
            m_approaching = 0;
            m_skippedNode = 0;
        }
        m_nextRepathTime = 1.0f + AINow();
    } else {
        CopyPos(m_destination, m_pathGoal);
    }
}

void CAIObject::WalkAStarPath() {
    UpdateAStarPathWalk();
    float dist;
    if (m_movePointType == AIMOVEPOINT_3D) {
        dist = m_position.GetDistanceXYZReal(m_pathGoal);
    } else {
        dist = m_position.GetDistanceXYReal(m_pathGoal);
    }
    if (dist - m_moveArriveDist < 0.5f) {
        m_pathDone = 1;
        BSObjectTriggerEvent(GetScript(), AIEVT_ARRIVED, NULL, NULL, false);
    }
}

// Queues the usable neighbours of `node`. Returns the new search node if
// one is the goal.
CSearchNode* CAIObject::AddChildrenToSearchNodeQueue(CSearchNode* node, const CPathNode* goal,
                                                     CSearchNodeQueue& queue, CSearchNodeList&) {
    CPathNode* pathNode = node->m_pPathNode;
    s16* link = pathNode->m_links;
    s16* end = link + pathNode->m_numLinks;
    for (; link < end; link++) {
        CPathNode* child = g_aigAIFilterGlobalObject.GetAStarPathNodeByIndex(*link);
        if (!child->m_enabled) {
            continue;
        }
        if (node->m_pParent != NULL && node->m_pParent->m_pPathNode == child) {
            continue;
        }
        CSearchNode* n = new CSearchNode;
        CSearchNode* existing = n->Setup(node, child, goal);
        if (existing != NULL && !existing->m_flag) {
            // Already queued: keep the cheaper.
            if (n->m_totalCost < existing->m_totalCost) {
                n->m_pPathNode->m_pSearchNode = n;
                queue.ReplaceElement(existing, n);
                delete existing;
            } else {
                delete n;
            }
        } else if (existing != NULL) {
            delete n;
        } else {
            if (goal == child) {
                return n;
            }
            queue.Insert(n);
        }
    }
    return NULL;
}

void CAIObject::CleanupAStarPathWalk() {
    CLinkedListElement* e = m_path.m_pHead;
    while (e != NULL) {
        m_path.RemoveElement(e);
        delete (CSearchNode*)e;
        e = m_path.m_pHead;
    }
}

// The node after `from` on the way to `goal` (NULL keeps the current goal).
// The path is only recomputed when the goal changes; a node whose
// successor is in plain reach is skipped.
CPathNode* CAIObject::GetNextPathNode(CPathNode* from, CPathNode* goal) {
    CSearchNode* first = (CSearchNode*)m_path.m_pHead;
    if (goal == m_pGoalNode && first != NULL) {
        m_path.RemoveElement(first);
        delete first;
        CSearchNode* next = (CSearchNode*)m_path.m_pHead;
        if (next != NULL) {
            m_pNextNode = next->m_pPathNode;
        } else {
            m_pNextNode = NULL;
        }
        return m_pNextNode;
    }
    if (goal != NULL) {
        m_pGoalNode = goal;
    }
    bool found = ComputeAStarPath(from, m_pGoalNode, 100.0f, *(CSearchNodeQueue*)SPR_BEGIN, m_path);
    CSearchNode* head = (CSearchNode*)m_path.m_pHead;
    CSearchNode* second = (CSearchNode*)head->m_pNext;
    if (second != NULL) {
        CAIFilterRealPosition start;
        CopyPos(start, m_position);
        start.field_0x8 += 0.4f;
        CPathNode* p = second->m_pPathNode;
        CAIFilterRealPosition end;
        end.field_0x0 = p->m_pos[0];
        end.field_0x4 = p->m_pos[1];
        end.field_0x8 = p->m_pos[2];
        end.field_0x8 += 0.4f;
        if (!CAIFilterGlobal::CheckCollisionLineToEnvironment(start, end, m_pFilter)) {
            m_path.RemoveElement(head);
            delete head;
            m_pNextNode = second->m_pPathNode;
            return m_pNextNode;
        }
    }
    m_pNextNode = head->m_pPathNode;
    if (found) {
        return m_pNextNode;
    }
    return NULL;
}

// A* from `start` to `goal`, giving up past `maxCost`. `path` gets the
// nodes from start to goal (or to the node closest to the goal). Returns
// whether the goal was reached.
bool CAIObject::ComputeAStarPath(CPathNode* start, const CPathNode* goal, float maxCost, CSearchNodeQueue& queue,
                                 CSearchNodeList& path) {
    queue.m_count = 1;
    CSearchNode* found = NULL;
    while (!path.IsEmpty()) {
        CLinkedListElement* e = path.m_pHead;
        path.RemoveElement(e);
        delete (CSearchNode*)e;
    }
    if (start == goal) {
        found = new CSearchNode;
        found->Setup(NULL, start, goal);
    } else {
        CSearchNode* n = new CSearchNode;
        n->Setup(NULL, start, goal);
        queue.Insert(n);
        float minCost = 0.25f * maxCost;
        while (queue.m_count > 1) {
            CSearchNode* best = queue.PopFirst();
            if (best->m_totalCost > maxCost && best->m_costFromStart > minCost) {
                delete best;
                break;
            }
            best->m_flag = true;
            path.AddElementAtEnd(best);
            found = AddChildrenToSearchNodeQueue(best, goal, queue, path);
            if (found != NULL) {
                break;
            }
        }
        queue.Clear();
    }
    bool reached = found != NULL;
    if (found != NULL) {
        if (!found->m_flag) {
            found->m_flag = true;
        }
        path.AddElementAtEnd(found);
    } else {
        // Settle for the visited node closest to the goal.
        float bestCost = 0.0f;
        CSearchNode* best = NULL;
        for (CSearchNode* e = (CSearchNode*)path.m_pHead; e != NULL; e = (CSearchNode*)e->m_pNext) {
            float cost = e->m_costToGoal;
            if (best == NULL || cost < bestCost) {
                best = e;
                bestCost = cost;
            }
        }
        found = best;
    }
    if (found != NULL) {
        // Unmark the chain back to the start, free everything else and
        // rebuild the list as the chain.
        CSearchNode* n;
        for (n = found; n != NULL; n = n->m_pParent) {
            n->m_pPathNode->m_pSearchNode = NULL;
            n->m_flag = !n->m_flag;
        }
        CSearchNode* e = (CSearchNode*)path.m_pHead;
        while (e != NULL) {
            CSearchNode* next = (CSearchNode*)e->m_pNext;
            path.RemoveElement(e);
            if (e->m_flag) {
                e->m_flag = !e->m_flag;
                delete e;
            }
            e = next;
        }
        for (n = found; n != NULL; n = n->m_pParent) {
            path.AddElementAtBeginning(n);
        }
    }
    return reached;
}

// Takes the closest usable cover point (from the target, if any).
CCoverPoint* CAIObject::StoreClosestCoverPoint() {
    CAIFilterRealPosition* threat;
    if (m_pTarget != NULL) {
        threat = &m_pTarget->m_position;
    } else {
        threat = NULL;
    }
    CAIFilterRealPosition eye;
    CopyPos(eye, m_position);
    eye.field_0x8 += 0.5f;
    float range = 0.5f * m_sightRange;
    range *= range;
    COVERPOINT_WALK_MODE mode;
    MathFunRandomI64(0, 1);
    m_pCoverPoint = g_aigAIFilterGlobalObject.GetClosestUsableCoverPoint(
        AILeaf(m_pFilter), eye, threat, &mode, (COVERPOINT_TYPE*)&m_coverType, &m_coverPosition, range, this);
    if (m_pCoverPoint != NULL) {
        if (!m_pCoverPoint->FlagAsOccupied((COVERPOINT_TYPE)m_coverType)) {
            BSObjectTriggerEvent(GetScript(), AIEVT_COVER_LOST, NULL, NULL, false);
        }
        m_peekPosition = m_pCoverPoint->GetPeekLocation();
        return m_pCoverPoint;
    }
    return NULL;
}

// Picks a cover point of `type` against `threat` (or the target); also kept
// in m_pChosenCoverPoint.
CCoverPoint* CAIObject::ChooseCoverPoint(ECoverPointSelectionType type, const ISceneNode* threat, float range,
                                 float maxDist) {
    CAIFilterRealPosition threatPos;
    CAIFilterRealPosition* from;
    if (threat != NULL) {
        from = &threatPos;
        threat->GetPosition(*(CVector3*)from);
    } else if (m_pTarget != NULL) {
        from = &m_pTarget->m_position;
    } else {
        from = NULL;
    }
    float r = 0.5f * range;
    CAIFilterRealPosition eye;
    CopyPos(eye, m_position);
    float rangeSq = r * r;
    eye.field_0x8 += 0.5f;
    CCoverPoint* point;
    if (from != NULL) {
        point = g_aigAIFilterGlobalObject.ChooseCoverPointByType(AILeaf(m_pFilter), eye, from, type, rangeSq,
                                                                 maxDist * maxDist, this);
    } else {
        point = g_aigAIFilterGlobalObject.GetClosestUsableCoverPoint(AILeaf(m_pFilter), eye, NULL, NULL, NULL,
                                                                     NULL, rangeSq, this);
    }
    m_pChosenCoverPoint = point;
    return point;
}

// Takes cover point `index` (-1: none), releasing the current one.
void CAIObject::SetCoverPoint(int index, bool checkUseful) {
    if (m_pCoverPoint != NULL) {
        field_0x1A8 &= ~2;
        if (m_pCoverPoint != NULL) {
            m_pCoverPoint->FlagAsUnoccupied((COVERPOINT_TYPE)m_coverType);
            m_pCoverPoint = NULL;
            m_moveMode = AIMOVE_NONE;
            m_atCoverPoint = 0;
        }
    }
    m_atCoverPoint = 0;
    m_checkCoverUseful = checkUseful;
    if (index >= 0) {
        m_pCoverPoint = g_aigAIFilterGlobalObject.GetCoverPointByIndex(index);
        if (m_pCoverPoint != NULL) {
            m_coverType = m_pCoverPoint->GetAvailablePeekPosition();
            CAIFilterRealVector3 pos;
            m_pCoverPoint->GetModifiedPosition(pos, (COVERPOINT_TYPE)m_coverType);
            if (!m_pCoverPoint->FlagAsOccupied((COVERPOINT_TYPE)m_coverType)) {
                BSObjectTriggerEvent(GetScript(), AIEVT_COVER_LOST, NULL, NULL, false);
            }
            m_peekPosition = m_pCoverPoint->GetPeekLocation();
            if (m_position.GetDistanceXYZReal(*(CAIFilterRealPosition*)&pos) < 0.3f) {
                m_atCoverPoint = 1;
            }
        }
    }
}

static inline void CommitWalk(CAIObject* self) {
    self->m_lastDistSq = self->m_position.GetDistanceSquaredXYReal(self->m_destination);
    self->m_approaching = 0;
    self->m_skippedNode = 0;
    self->m_nextRepathTime = 1.0f + AINow();
    self->m_nextWalkUpdate = 0.3f + AINow();
    self->EngageObstacleAvoidance(false, 0.0f, 0.0f, 0.0f);
}

// Plans an A* walk to the cover point, arriving facing into the cover;
// `commit` starts it.
bool CAIObject::StartCoverPointWalk(bool commit) {
    m_atCoverPoint = 0;
    m_moveMode = AIMOVE_NONE;
    StopPathWalk(this);
    m_pCoverPoint->GetModifiedPosition(*(CAIFilterRealVector3*)&m_movePoint, (COVERPOINT_TYPE)m_coverType);
    CopyPos(m_coverPosition, m_movePoint);
    CAIFilterRealVector3 startDir;
    startDir.field_0x8 = 0.0f;
    startDir.field_0x4 = 0.0f;
    startDir.field_0x0 = 0.0f;
    CAIFilterRealVector3 endDir;
    GetFacing((const f32*)&m_pCoverPoint->m_data->orientation, endDir);
    bool ok = true;
    endDir.field_0x0 *= -1.0f;
    endDir.field_0x4 *= -1.0f;
    endDir.field_0x8 *= -1.0f;
    m_movePointType = 0;
    if (!SetupAStarPathWalk(startDir, endDir, false)) {
        ok = false;
        CopyPos(m_destination, m_movePoint);
    }
    if (commit) {
        CommitCoverPointWalkToStored();
    } else {
        CommitWalk(this);
    }
    if (!m_pCoverPoint->FlagAsOccupied((COVERPOINT_TYPE)m_coverType)) {
        BSObjectTriggerEvent(GetScript(), AIEVT_COVER_LOST, NULL, NULL, false);
    }
    return ok;
}

void CAIObject::CommitCoverPointWalkToStored() {
    CommitWalk(this);
    m_moveMode = AIMOVE_COVER;
    m_pFilter->SetMoveTarget(m_destination);
    m_coverFlags &= ~(AICOVER_ARRIVED | AICOVER_NEAR | AICOVER_STEP);
}

bool CAIObject::StartCoverPointWalkToStored(bool commit) {
    return StartCoverPointWalk(commit);
}

// Tells the script when the cover point is gone or (every 0.2 s) no longer
// covers from the target.
void CAIObject::CheckCoverpointUsefulness() {
    if (m_checkCoverUseful) {
        if (m_pCoverPoint == NULL) {
            BSObjectTriggerEvent(GetScript(), AIEVT_COVER_LOST, NULL, NULL, true);
            return;
        }
        if (!(AINow() < m_nextCoverCheckTime)) {
            m_nextCoverCheckTime = 0.2f + AINow();
            if (m_pTarget != NULL) {
                CAIFilterRealVector3 pos;
                m_pCoverPoint->GetModifiedPosition(pos, (COVERPOINT_TYPE)m_coverType);
                if (!m_pCoverPoint->ProvidesCover(*(CAIFilterRealPosition*)&pos, &m_pTarget->m_position, this)) {
                    BSObjectTriggerEvent(GetScript(), AIEVT_COVER_LOST, NULL, NULL, true);
                }
            }
        }
    }
}

static inline void SendNearCoverEvent(CAIObject* self, int normal, int high) {
    u32 type = self->m_coverType;
    if (type & 0xC0) {
        BSObjectTriggerEvent(self->GetScript(), high, ADDR_TO_PTR(void, type), NULL, false);
    } else {
        BSObjectTriggerEvent(self->GetScript(), normal, ADDR_TO_PTR(void, type), NULL, false);
    }
}

// Walks the A* path to the cover point. Within about 5 units, if the cover
// is ahead and the path is not much longer than the straight line, goes
// straight in; within about 1 unit takes the last step; at 0.3 has arrived.
void CAIObject::WalkToCoverPoint() {
    if (!(AINow() < m_nextWalkUpdate)) {
        m_nextWalkUpdate = 0.1f + AINow();
        CheckCoverpointUsefulness();
        if (m_pCoverPoint == NULL) {
            return;
        }
    }
    UpdateAStarPathWalk();
    if (m_pCoverPoint == NULL) {
        return;
    }
    float dist = m_position.GetDistanceXYZReal(m_coverPosition);
    CAIFilterRealVector3 facing;
    GetFacing((const f32*)&m_pCoverPoint->m_data->orientation, facing);
    CAIFilterRealVector3 dir;
    dir.field_0x4 = m_coverPosition.field_0x4 - m_position.field_0x4;
    dir.field_0x0 = m_coverPosition.field_0x0 - m_position.field_0x0;
    dir.field_0x8 = m_coverPosition.field_0x8 - m_position.field_0x8;
    CAIFilterRealVector3 side = m_side;
    Normalize(dir);
    float into = facing.field_0x8 * dir.field_0x8 + (facing.field_0x0 * dir.field_0x0 + facing.field_0x4 * dir.field_0x4);

    if (!(m_coverFlags & AICOVER_NEAR) && MathFunCloseToZero(dist - 4.9f, 0.3f)) {
        if (MathFunCloseToZero(dist - 4.9f - 0.6f, 0.3f)) {
            m_coverFlags |= AICOVER_NEAR;
        } else if (into > 0.01f &&
                   side.field_0x8 * dir.field_0x8 + (side.field_0x0 * dir.field_0x0 + side.field_0x4 * dir.field_0x4) >
                       0.7071f) {
            float pathLen;
            CLinkedListElement* prev = m_path.m_pHead;
            if (prev == NULL) {
                pathLen = 0.0f;
            } else {
                pathLen = 0.0f;
                for (CSearchNode* n = (CSearchNode*)prev->m_pNext; n != NULL; n = (CSearchNode*)n->m_pNext) {
                    pathLen += n->GetCostValue((CSearchNode*)prev);
                    prev = n;
                }
            }
            if (pathLen < 1.25f * dist) {
                CAIFilterRealPosition end;
                end = m_coverPosition;
                end.field_0x8 += 0.5f;
                CAIFilterRealPosition start;
                start = m_position;
                start.field_0x8 += 0.5f;
                if (!CAIFilterGlobal::CheckCollisionLineToEnvironment(start, end, m_pFilter)) {
                    SendNearCoverEvent(this, AIEVT_NEAR_COVER, AIEVT_NEAR_COVER_HIGH);
                    m_pathDone = 1;
                    m_atCoverPoint = 1;
                    m_coverFlags |= AICOVER_NEAR;
                    m_pCoverPoint->GetModifiedPosition(*(CAIFilterRealVector3*)&m_coverPosition,
                                                       (COVERPOINT_TYPE)m_coverType);
                    CopyPos(m_destination, m_coverPosition);
                    m_pFilter->SetMoveTarget(m_destination);
                }
            }
        }
    }
    if (!(m_coverFlags & AICOVER_STEP) && MathFunCloseToZero(dist - 0.8f, 0.2f)) {
        if (MathFunCloseToZero(dist - 0.8f - 0.4f, 0.2f)) {
            m_coverFlags |= AICOVER_STEP;
        } else if (into > 0.01f) {
            SendNearCoverEvent(this, AIEVT_COVER_STEP, AIEVT_COVER_STEP_HIGH);
            m_pathDone = 1;
            m_atCoverPoint = 1;
            m_coverFlags |= AICOVER_STEP;
            m_pCoverPoint->GetModifiedPosition(*(CAIFilterRealVector3*)&m_coverPosition,
                                               (COVERPOINT_TYPE)m_coverType);
            CopyPos(m_destination, m_coverPosition);
            m_pFilter->SetMoveTarget(m_destination);
        }
    }
    if (!(m_coverFlags & AICOVER_ARRIVED) && dist < 0.3f && into > 0.01f) {
        m_atCoverPoint = 1;
        BSObjectTriggerEvent(GetScript(), AIEVT_AT_COVER, ADDR_TO_PTR(void, m_coverType), NULL, true);
        m_coverFlags |= AICOVER_ARRIVED;
    }
}

// Straight to the cover point (already close).
void CAIObject::WalkToCoverPointDirect() {
    if (!(AINow() < m_nextWalkUpdate)) {
        m_nextWalkUpdate = 0.1f + AINow();
        CheckCoverpointUsefulness();
    }
    float distSq = m_position.GetDistanceSquaredXYZReal(m_destination);
    if (!(m_coverFlags & AICOVER_NEAR) && MathFunCloseToZero(distSq - 24.01f, 0.09f)) {
        SendNearCoverEvent(this, AIEVT_NEAR_COVER, AIEVT_NEAR_COVER_HIGH);
        m_coverFlags |= AICOVER_NEAR;
    }
    if (!(m_coverFlags & AICOVER_ARRIVED) && distSq < 0.09f) {
        m_atCoverPoint = 1;
        BSObjectTriggerEvent(GetScript(), AIEVT_AT_COVER, ADDR_TO_PTR(void, m_coverType), NULL, true);
        m_coverFlags |= AICOVER_ARRIVED;
    }
}

// Walking to the step point next to the cover; then the last step in.
void CAIObject::WalkToCoverPointIndirect() {
    if (!(AINow() < m_nextWalkUpdate)) {
        m_nextWalkUpdate = 0.1f + AINow();
        CheckCoverpointUsefulness();
    }
    float distSq = m_position.GetDistanceSquaredXYZReal(m_destination);
    if (!(m_coverFlags & AICOVER_STEP) && distSq < 0.040000003f) {
        SendNearCoverEvent(this, AIEVT_COVER_STEP, AIEVT_COVER_STEP_HIGH);
        m_coverFlags |= AICOVER_STEP;
        m_moveMode = AIMOVE_COVER_INDIRECT;
        m_pCoverPoint->GetModifiedPosition(*(CAIFilterRealVector3*)&m_destination, (COVERPOINT_TYPE)m_coverType);
        m_pFilter->SetMoveTarget(m_destination);
    }
}

void CAIObject::SetupArbitraryPointUpdate(EMovePointType type, BS_STRUCT_Vector_struct* var, float arriveDist) {
    m_movePointType = type;
    m_moveArriveDist = arriveDist;
    m_pMovePointVar = var;
}

void CAIFilterObject::SetMoveTarget(const CAIFilterRealPosition&) {}

void CAIObject::StartWalkToArbitraryPoint(CAIFilterRealPosition& point) {
    m_moveMode = AIMOVE_NONE;
    StopPathWalk(this);
    m_moveMode = AIMOVE_ARBITRARY;
    CopyPos(m_movePoint, point);
    CopyPos(m_destination, m_movePoint);
    m_pFilter->SetMoveTarget(m_destination);
}

void CAIObject::WalkToArbitraryPoint() {
    if (AINow() < m_nextWalkUpdate) {
        UpdateMovePointFromTarget(this);
        CopyPos(m_destination, m_movePoint);
    } else {
        m_nextWalkUpdate = 0.1f + AINow();
    }
    float dist;
    if (m_movePointType == AIMOVEPOINT_3D) {
        dist = m_position.GetDistanceXYZReal(m_destination);
    } else {
        dist = m_position.GetDistanceXYReal(m_destination);
    }
    if (dist - m_moveArriveDist < 0.5f) {
        BSObjectTriggerEvent(GetScript(), AIEVT_ARRIVED, NULL, NULL, false);
    }
}

void CAIObject::ProcessObstacleAvoidance() {
    m_avoidance.m_target.field_0x0 = m_destination.field_0x0;
    m_avoidance.m_target.field_0x4 = m_destination.field_0x4;
    m_avoidance.m_target.field_0x8 = m_destination.field_0x8;
    m_avoidance.RunEnvironmentCollisionChecks();
    m_avoidance.DetermineNewObjectivePosition(m_avoidPosition);
}

static inline float DegToRad(float fov) {
    return 0.017453292f * fov;
}

void CAIObject::SetupFieldOfViewModeObject(CAIObject* obj, float range, float fov) {
    m_pVisionObject = obj;
    m_objectSightRange = CheckSightRange(range, "Warning: SFOVMO - Vision distance is zero, changing to default\n");
    if (fov <= 0.001f) {
        fov = 90.0f;
    }
    m_objectFov = DegToRad(fov);
}

void CAIObject::SetupFieldOfViewModeList(aistatus_match* list, int count, float range, float fov) {
    memcpy(m_visionMatches, list, count * sizeof(aistatus_match));
    m_numVisionMatches = count;
    m_pVisionCursor = NULL;
    m_listSightRange = CheckSightRange(range, "Warning: SFOVML - Vision distance is zero, changing to default\n");
    if (fov <= 0.001f) {
        fov = 90.0f;
    }
    m_listFov = DegToRad(fov);
}

void CAIObject::SetupFieldOfViewCorpse(float range, float fov) {
    m_corpseSightRange = CheckSightRange(range, "Warning: SFOVC - Vision distance is zero, changing to default\n");
    if (fov <= 0.001f) {
        printf("Warning: FOV is zero, changing to default\n");
        fov = 90.0f;
    }
    m_corpseFov = DegToRad(fov);
}

CAIObject* CAIObject::CheckFieldOfViewModeObject() {
    CAIObject* obj = m_pVisionObject;
    float fov = m_objectFov;
    float range = TargetSightRange(obj, m_objectSightRange);
    if (IsObjectInFieldOfView(obj->m_position, range, fov, obj->IsCrouching())) {
        CAIObject* seen = m_pVisionObject;
        if (seen == m_pTarget) {
            CopyPos(m_lastSeenPosition, seen->m_position);
            m_targetVisible = 1;
            m_targetSeen = 1;
        }
        return m_pVisionObject;
    }
    return NULL;
}

// The next object after the cursor (round the list) matching the vision
// status list, or NULL after a full round.
static inline CAIObject* NextVisionCandidate(CAIObject* self) {
    CAIObject* head = g_paioAIObjectListStart;
    CAIObject* obj = self->m_pVisionCursor != NULL ? self->m_pVisionCursor : head;
    for (;;) {
        obj = obj->m_pNext;
        if (obj == NULL) {
            obj = head;
        }
        if (obj != self && obj->m_status.MatchStatusList(self->m_visionMatches, self->m_numVisionMatches)) {
            self->m_pVisionCursor = obj;
            return self->m_pVisionCursor;
        }
        if (self->m_pVisionCursor != NULL) {
            if (obj == self->m_pVisionCursor) {
                return NULL;
            }
        } else if (obj == head) {
            return NULL;
        }
    }
}

CAIObject* CAIObject::CheckFieldOfViewModeList() {
    CAIObject* obj = NextVisionCandidate(this);
    if (obj == NULL) {
        return NULL;
    }
    float fov = m_listFov;
    float range = TargetSightRange(obj, m_listSightRange);
    if (IsObjectInFieldOfView(obj->m_position, range, fov, obj->IsCrouching())) {
        return obj;
    }
    return NULL;
}

int CAIStatus::GetVarValue(aistatus_var var) const {
    int value = 0;
    switch (var) {
    case AISTATUS_HEALTH:
    case AISTATUS_VAR1:
    case AISTATUS_VAR2:
    case AISTATUS_THREAT:
    case AISTATUS_VAR4:
    case AISTATUS_TEAM:
    case AISTATUS_ATTACKERS:
    case AISTATUS_DEAD:
        value = m_vars[var];
        break;
    case AISTATUS_VISIBILITY:
        value = 0;
        break;
    case AISTATUS_DISTANCE:
        value = s_paioAttacker->ComputeStatusDistance(s_paioTarget);
        break;
    case AISTATUS_THREAT_VALUE:
        value = ThreatValue(s_paioAttacker, s_paioTarget, s_paioTarget->m_status.m_vars[AISTATUS_THREAT],
                            s_paioTarget->m_status.m_vars[AISTATUS_ATTACKERS]);
        break;
    case AISTATUS_LOS:
        value = s_paioAttacker->ComputeStatusLOSValue(s_paioTarget);
        break;
    case AISTATUS_EXP_DISTANCE:
        value = s_paioAttacker->ComputeStatusExponentialDistance(s_paioTarget);
        break;
    case AISTATUS_IS_ENEMY:
        value = s_paioAttacker->ComputeStatusIsEnemy(s_paioTarget);
        break;
    }
    return value;
}

int CAIObject::GetAIStatusVar(aistatus_var var) const {
    int value = 0;
    switch (var) {
    case AISTATUS_HEALTH:
    case AISTATUS_VAR1:
    case AISTATUS_VAR2:
    case AISTATUS_THREAT:
    case AISTATUS_VAR4:
    case AISTATUS_TEAM:
    case AISTATUS_ATTACKERS:
    case AISTATUS_DEAD:
        value = m_status.m_vars[var];
        break;
    case AISTATUS_DISTANCE:
        value = CAIStatus::s_paioAttacker->ComputeStatusDistance(CAIStatus::s_paioTarget);
        break;
    case AISTATUS_THREAT_VALUE: {
        CAIObject* target = CAIStatus::s_paioTarget;
        CAIObject* attacker = CAIStatus::s_paioAttacker;
        int threat = target->m_status.GetVarValue((aistatus_var)AISTATUS_THREAT);
        int exp = attacker->ComputeStatusExponentialDistance(target);
        value = (threat + (15 - exp) * 2) / (target->m_status.GetVarValue((aistatus_var)AISTATUS_ATTACKERS) + 3);
        if (value < 0) {
            value = 0;
        } else if (value > 15) {
            value = 15;
        }
        break;
    }
    case AISTATUS_LOS: {
        CAIObject* target = CAIStatus::s_paioTarget;
        CAIObject* attacker = CAIStatus::s_paioAttacker;
        float range = TargetSightRange(target, attacker->m_sightRange);
        value = attacker->IsObjectInLineOfSight(target->m_position, range, target->IsCrouching()) ? 15 : 0;
        break;
    }
    case AISTATUS_EXP_DISTANCE:
        value = CAIStatus::s_paioAttacker->ComputeStatusExponentialDistance(CAIStatus::s_paioTarget);
        break;
    case AISTATUS_IS_ENEMY:
        value = AIIsEnemyTeam(CAIStatus::s_paioAttacker->field_0x14, CAIStatus::s_paioTarget->field_0x14) ? 15 : 0;
        break;
    }
    return value;
}

// The first dead object in the corpse field of view.
CAIObject* CAIObject::CheckFieldOfViewCorpse() {
    for (CAIObject* obj = g_paioAIObjectListStart; obj != NULL; obj = obj->m_pNext) {
        if (obj->GetAIStatusVar((aistatus_var)AISTATUS_DEAD) != 0) {
            float fov = m_corpseFov;
            float range = TargetSightRange(obj, m_corpseSightRange);
            if (IsObjectInFieldOfView(obj->m_position, range, fov, obj->IsCrouching())) {
                return obj;
            }
        }
    }
    return NULL;
}

// In range, within `fov` of the look direction and in line of sight.
bool CAIObject::IsObjectInFieldOfView(CAIFilterRealPosition& pos, float range, float fov, bool crouching) {
    if (fov <= 0.001f) {
        fov = 1.5707964f;
    }
    range = CheckSightRange(range, "Warning: IOIFOV - Vision distance is zero, changing to default\n");
    if ((m_flags & AIFLAG_BLIND) == AIFLAG_BLIND) {
        return false;
    }
    range = CheckSightRange(range, "Warning: IOIVD - Vision distance is zero, changing to default\n");
    if (!(m_position.GetDistanceXYZReal(pos) <= range)) {
        return false;
    }
    CAIFilterRealVector3 look;
    CAIFilterRealPosition eye;
    m_pFilter->GetLookDirection(look, *(CAIFilterRealVector3*)&eye);
    CAIFilterRealPosition at;
    at.field_0x8 = pos.field_0x8;
    at.field_0x8 += 0.8f;
    at.field_0x0 = pos.field_0x0;
    at.field_0x4 = pos.field_0x4;
    CAIFilterRealVector3 dir;
    dir.field_0x0 = at.field_0x0 - eye.field_0x0;
    dir.field_0x4 = at.field_0x4 - eye.field_0x4;
    dir.field_0x8 = at.field_0x8 - eye.field_0x8;
    Normalize(dir);
    Normalize(look);
    if (dir.field_0x8 * look.field_0x8 + (dir.field_0x0 * look.field_0x0 + dir.field_0x4 * look.field_0x4) <
        (float)cos(fov)) {
        return false;
    }
    return CanSeeFrom(eye, pos, crouching, m_pFilter);
}

void CAIObject::SetupLineOfSightModeObject(CAIObject* obj, float range) {
    m_pVisionObject = obj;
    m_objectSightRange = CheckSightRange(range, "Warning: SLOSMO - Vision distance is zero, changing to default\n");
}

void CAIObject::SetupLineOfSightModeList(aistatus_match* list, int count, float range) {
    memcpy(m_visionMatches, list, count * sizeof(aistatus_match));
    m_numVisionMatches = count;
    m_pVisionCursor = NULL;
    m_listSightRange = CheckSightRange(range, "Warning: SLOSML - Vision distance is zero, changing to default\n");
}

// From the peek position when at a cover point, else as any object.
bool CAIObject::IsTargetInLineOfSight(float range) {
    if (m_pCoverPoint != NULL && m_atCoverPoint) {
        bool crouching = m_pTarget->IsCrouching();
        CAIFilterRealPosition& pos = m_pTarget->m_position;
        bool seen;
        if ((m_flags & AIFLAG_BLIND) == AIFLAG_BLIND) {
            seen = false;
        } else {
            float r = CheckSightRange(range, "Warning: IOIVD - Vision distance is zero, changing to default\n");
            if (!(m_position.GetDistanceXYZReal(pos) <= r)) {
                seen = false;
            } else {
                seen = CanSeeFrom(m_peekPosition, pos, crouching, m_pFilter);
            }
        }
        if (seen) {
            return seen;
        }
        CAIObject* target = m_pTarget;
        return IsObjectInLineOfSight(target->m_position, TargetSightRange(target, range), target->IsCrouching());
    }
    CAIObject* target = m_pTarget;
    return IsObjectInLineOfSight(target->m_position, TargetSightRange(target, range), target->IsCrouching());
}

CAIObject* CAIObject::CheckLineOfSightModeObject() {
    CAIObject* obj = m_pVisionObject;
    if (obj == NULL) {
        return NULL;
    }
    bool seen;
    if (obj == m_pTarget) {
        seen = IsTargetInLineOfSight(m_objectSightRange);
        if (seen) {
            CopyPos(m_lastSeenPosition, m_pVisionObject->m_position);
            m_targetVisible = 1;
            m_targetSeen = 1;
        } else {
            m_targetVisible = 0;
        }
    } else {
        float range = TargetSightRange(obj, m_objectSightRange);
        seen = IsObjectInLineOfSight(obj->m_position, range, obj->IsCrouching());
    }
    if (seen) {
        return m_pVisionObject;
    }
    return NULL;
}

CAIObject* CAIObject::CheckLineOfSightModeList() {
    CAIObject* obj = NextVisionCandidate(this);
    if (obj == NULL) {
        return NULL;
    }
    float range = TargetSightRange(obj, m_listSightRange);
    if (IsObjectInLineOfSight(obj->m_position, range, obj->IsCrouching())) {
        return obj;
    }
    return NULL;
}

void CAIFilterObject::GetLookDirection(CAIFilterRealVector3&, CAIFilterRealVector3&) {}

bool CAIObject::IsObjectInLineOfSight(CAIFilterRealPosition& pos, float range, bool crouching) {
    if ((m_flags & AIFLAG_BLIND) == AIFLAG_BLIND) {
        return false;
    }
    range = CheckSightRange(range, "Warning: IOIVD - Vision distance is zero, changing to default\n");
    if (!(m_position.GetDistanceXYZReal(pos) <= range)) {
        return false;
    }
    CAIFilterRealVector3 look;
    CAIFilterRealPosition eye;
    m_pFilter->GetLookDirection(look, *(CAIFilterRealVector3*)&eye);
    return CanSeeFrom(eye, pos, crouching, m_pFilter);
}

// Looks for `obj` around `pos` (e.g. where a sound came from): checks
// random spots across a patch facing us.
void CAIObject::SetupVisionSeekingMode(CAIObject* obj, CAIFilterRealPosition& pos, float range) {
    CopyPos(m_seekPosition, pos);
    m_pVisionObject = obj;
    m_objectSightRange = CheckSightRange(range, "Warning: SVSM - Vision distance is zero, changing to default\n");
    CAIFilterRealPosition eye;
    CopyPos(eye, m_position);
    eye.field_0x8 += 1.5f;
    m_seekSide.field_0x0 = pos.field_0x0 - eye.field_0x0;
    m_seekSide.field_0x4 = pos.field_0x4 - eye.field_0x4;
    m_seekSide.field_0x8 = pos.field_0x8 - eye.field_0x8;
    m_seekSide.field_0x8 = 0.0f;
    // Perpendicular, in the horizontal plane.
    float x = -m_seekSide.field_0x0;
    m_seekSide.field_0x0 = m_seekSide.field_0x4;
    m_seekSide.field_0x4 = x;
    Normalize(m_seekSide);
    m_seekOffsetSide = -0.25f;
    m_seekOffsetUp = -0.75f;
}

CAIObject* CAIObject::CheckVisionSeekingMode() {
    for (int i = 0; i < 2; i++) {
        m_seekOffsetSide = MathFunRandomRealSigned(-0.25f, 0.25f);
        m_seekOffsetUp = MathFunRandomRealSigned(-0.75f, 0.75f);
        CAIFilterRealPosition spot;
        spot.field_0x0 = m_seekOffsetSide * m_seekSide.field_0x0 + m_seekPosition.field_0x0;
        spot.field_0x4 = m_seekOffsetSide * m_seekSide.field_0x4 + m_seekPosition.field_0x4;
        spot.field_0x8 = m_seekOffsetUp + m_seekPosition.field_0x8;
        CAIFilterRealPosition eye;
        CopyPos(eye, m_position);
        eye.field_0x8 += 1.5f;
        if (!CAIFilterGlobal::CheckVisionLineToEnvironment(eye, spot, m_pFilter)) {
            float range = CheckSightRange(m_objectSightRange,
                                          "Warning: IOIVD - Vision distance is zero, changing to default\n");
            if (m_position.GetDistanceXYZReal(m_seekPosition) <= range) {
                return m_pVisionObject;
            }
        }
    }
    return NULL;
}

bool CAIObject::IsObjectReachable(const CAIFilterRealPosition& pos) {
    CAIFilterRealPosition start;
    start = m_position;
    CAIFilterRealPosition end;
    start.field_0x8 += 0.4f;
    CopyPos(end, pos);
    end.field_0x8 += 0.4f;
    return !CAIFilterGlobal::CheckCollisionLineToEnvironment(start, end, m_pFilter);
}

// What a script gets with a heard sound (event block of 0x20 bytes).
struct AISoundHeardData {
    /* 0x00 */ f32 m_position[3];
    /* 0x0C */ PTR32(BSObject) m_source;
    /* 0x10 */ f32 m_loudness;  // volume / distance
    /* 0x14 */ u32 m_notFootstep;
    /* 0x18 */ s32 field_0x18;
    /* 0x1C */ s32 field_0x1C;  // SoundInfo field_0x28, or 0 from a friend
};
FILE_SIZE_ASSERT(AISoundHeardData, 0x20);  // an event memory block; scripts read it

// Tells every object in hearing range (scaled by the volume) about a sound,
// as a timer event after a random delay.
void CAIObject::PropagateSoundToAllObjects(const CAIFilterSoundInfo& info, CAIObject* list) {
    for (CAIObject* obj = list; obj != NULL; obj = obj->m_pNext) {
        if (info.m_sourceFilter == obj->m_pFilter) {
            continue;
        }
        float dy = obj->m_position.field_0x4 - info.m_position.field_0x4;
        float dx = obj->m_position.field_0x0 - info.m_position.field_0x0;
        float r = obj->m_hearingRadius * info.m_volume;
        float dz = obj->m_position.field_0x8 - info.m_position.field_0x8;
        float distSq = dz * dz + (dx * dx + dy * dy);
        if (distSq < r * r) {
            AISoundHeardData* data = (AISoundHeardData*)BSEventGetEventMemoryBlock(sizeof(AISoundHeardData));
            data->m_position[0] = info.m_position.field_0x0;
            data->m_position[1] = info.m_position.field_0x4;
            data->m_position[2] = info.m_position.field_0x8;
            data->m_source = info.m_sourceScript;
            data->m_loudness = info.m_volume * (1.0f / sqrtf(distSq));
            data->m_notFootstep = info.m_type != 0x33;
            data->field_0x18 = info.field_0x2C;
            if (info.m_sourceFilter != NULL &&
                !AIIsEnemyTeam(obj->field_0x14, info.m_sourceFilter->m_pAIObject->field_0x14)) {
                data->field_0x1C = 0;
            } else {
                data->field_0x1C = info.field_0x28;
            }
            BSRegisterTimerEvent((int)MathFunRandomReal(10.0f, 60.0f), info.m_type, obj->GetScript(), data,
                                 TIMER_KEEP_EARLIER);
        }
    }
}

// The closest free mounted gun within 25 that faces the target (within 30
// degrees) and has no enemy within 5 of it.
TriggerObject_struct* CAIObject::ChooseMMGPoint() {
    float bestDist = 25.0f;
    TriggerObject_struct* best = NULL;
    for (TriggerObject_struct* t = g_pTriggerObjects; t != g_pTriggerObjects + g_pNumTriggers; t++) {
        TriggerObjectData_struct* data = t->m_data;
        if (data->m_type != 5 || data->field_0x48 != (s32)MGPOINT_CRC || (t->m_flags & 1)) {
            continue;
        }
        CAIFilterRealPosition pos;
        pos.field_0x0 = data->m_position[0];
        pos.field_0x4 = data->m_position[1];
        pos.field_0x8 = data->m_position[2];
        CAIFilterRealVector3 facing;
        GetFacing(data->m_orientation, facing);
        Normalize(facing);
        CAIObject* target = m_pTarget;
        if (target != NULL) {
            CAIFilterRealVector3 dir;
            dir.field_0x4 = target->m_position.field_0x4 - pos.field_0x4;
            dir.field_0x0 = target->m_position.field_0x0 - pos.field_0x0;
            dir.field_0x8 = target->m_position.field_0x8 - pos.field_0x8;
            Normalize(dir);
            if (dir.field_0x8 * facing.field_0x8 + (dir.field_0x0 * facing.field_0x0 + dir.field_0x4 * facing.field_0x4) <
                0.866f) {
                continue;
            }
        }
        bool taken = false;
        for (CAIObject* obj = g_paioAIObjectListStart; obj != NULL; obj = obj->m_pNext) {
            if (obj != this && (obj->field_0x14 == 0 || obj->field_0x14 != field_0x14) &&
                pos.GetDistanceXYZReal(obj->m_position) < 5.0f) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            float dist = m_position.GetDistanceXYZReal(pos);
            if (dist < bestDist) {
                best = t;
                bestDist = dist;
            }
        }
    }
    return best;
}

// Whether the target has moved out of the gun's arc (30 degrees).
bool CAIObject::ShouldILeaveMMG(TriggerObject_struct* gun) {
    if (m_pTarget != NULL && m_targetVisible) {
        TriggerObjectData_struct* data = gun->m_data;
        float z = data->m_position[2];
        float y = data->m_position[1];
        float x = data->m_position[0];
        CAIFilterRealVector3 facing;
        GetFacing(data->m_orientation, facing);
        Normalize(facing);
        CAIObject* target = m_pTarget;
        CAIFilterRealVector3 dir;
        dir.field_0x4 = target->m_position.field_0x4 - y;
        dir.field_0x0 = target->m_position.field_0x0 - x;
        dir.field_0x8 = target->m_position.field_0x8 - z;
        Normalize(dir);
        if (dir.field_0x8 * facing.field_0x8 + (dir.field_0x0 * facing.field_0x0 + dir.field_0x4 * facing.field_0x4) <
            0.866f) {
            return true;
        }
    }
    return false;
}

// Layout checks against the original (offsets used by the AI units).
SIZE_ASSERT(CAIObject, 0x734);
SIZE_ASSERT(CObstacleAvoidance, 0x3A0);
typedef char AIObjectStatusCheck[sizeof(CAIStatus) == 0x38 ? 1 : -1];
