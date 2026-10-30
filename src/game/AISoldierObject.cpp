#include "game/AISoldierObject.h"
#ifdef FRONTLINE_NATIVE
#include "native/ai.h"
#endif

void* CAISoldierObject::operator new(unsigned long size, void* buffer) {
    return buffer;
}

CAISoldierObject::CAISoldierObject(void* gameObject) {
    m_position2.field_0x0 = 0.0f;
    m_position2.field_0x4 = 0.0f;
    m_position2.field_0x8 = 0.0f;
    m_pFilter = (CAIFilterObject*)gameObject;
    m_targetMode = 3;
    ResetTargetMatchList((aifilter_target_mode)3);
}

CAISoldierObject::~CAISoldierObject() {}

void CAISoldierObject::PreUpdate() {}

void CAISoldierObject::Update(float dt) {
#ifdef FRONTLINE_NATIVE
    if (NativeAIOn()) {
        NativeAISoldierUpdate(this);
    }
#endif
    if (UpdateReactionTime() && m_pTarget != NULL) {
        UpdateTargetPosition();
    }
    switch (m_moveMode) {
    case 1:
        WalkNextSplinePathPoint();
        break;
    case 2:
        WalkAStarPath();
        break;
    case 3:
        WalkToCoverPointDirect();
        break;
    case 4:
        WalkToCoverPointIndirect();
        break;
    case 5:
        CheckCoverpointUsefulness();
        break;
    case 8:
        WalkToArbitraryPoint();
        break;
    case 9:
        WalkToCoverPoint();
        break;
    }
    if (m_avoidObstacles) {
        ProcessObstacleAvoidance();
    }
    m_status.m_vars[AISTATUS_HEALTH] = AIHealthLevel(m_health, m_maxHealth);
    m_status.m_vars[AISTATUS_THREAT] = 4;
    m_status.m_vars[AISTATUS_VAR4] = 1;
}

void CAISoldierObject::Init() {
    CAIObject::Init();
    m_status.m_vars[AISTATUS_HEALTH] = AIHealthLevel(m_health, m_maxHealth);
    m_status.m_vars[AISTATUS_VAR1] = field_0x15;
    m_status.m_vars[AISTATUS_VAR2] = field_0x214;
    m_status.m_vars[AISTATUS_THREAT] = 4;
    m_status.m_vars[AISTATUS_VAR4] = 1;
    m_status.m_vars[AISTATUS_TEAM] = field_0x14;
    m_status.m_vars[AISTATUS_ATTACKERS] = 0;
    m_status.m_vars[AISTATUS_DEAD] = 0;
    m_targetMode = 3;
    ResetTargetMatchList((aifilter_target_mode)3);
}

// Status conditions a target must meet, per target mode (op 2 is <, 3 is >).
void CAISoldierObject::ResetTargetMatchList(aifilter_target_mode) {
    switch (m_targetMode) {
    case 2:
        SetStatusMatch(&m_targetMatches[0], 0, 9, 3);
        SetStatusMatch(&m_targetMatches[1], 3, 5, 3);
        SetStatusMatch(&m_targetMatches[2], 1, 5, 3);
        SetStatusMatch(&m_targetMatches[3], 2, 5, 3);
        m_numTargetMatches = 4;
        break;
    case 3:
        SetStatusMatch(&m_targetMatches[0], 0, 9, 3);
        SetStatusMatch(&m_targetMatches[1], 3, 5, 3);
        SetStatusMatch(&m_targetMatches[2], 1, 5, 3);
        SetStatusMatch(&m_targetMatches[3], 2, 5, 3);
        m_numTargetMatches = 4;
        break;
    case 4:
        SetStatusMatch(&m_targetMatches[0], 0, 4, 2);
        SetStatusMatch(&m_targetMatches[1], 1, 5, 2);
        m_numTargetMatches = 2;
        break;
    case 6:
        SetStatusMatch(&m_targetMatches[0], 0, 4, 2);
        SetStatusMatch(&m_targetMatches[1], 1, 5, 2);
        m_numTargetMatches = 2;
        break;
    case 5:
        SetStatusMatch(&m_targetMatches[0], 4, 5, 3);
        SetStatusMatch(&m_targetMatches[1], 4, 11, 2);
        SetStatusMatch(&m_targetMatches[2], 3, 4, 3);
        SetStatusMatch(&m_targetMatches[3], 1, 4, 3);
        m_numTargetMatches = 4;
        break;
    }
}

void CAISoldierObject::SetupTargetMatchList(aifilter_target_mode, aistatus_match* list,
                                            int count) {
    aistatus_match* dst = m_targetMatches;
    for (int i = 0; i < count; i++) {
        *dst++ = *list++;
    }
}

// Teams: 0 is hostile to 1 and 2; 1 and 2 are hostile to each other.
static inline bool IsEnemyTeam(int mine, int theirs) {
    switch (mine) {
    case 0:
        switch (theirs) {
        case 0:
            return false;
        case 1:
        case 2:
            return true;
        }
        return false;
    case 1:
        switch (theirs) {
        case 0:
        case 1:
            return false;
        case 2:
            return true;
        }
        return false;
    case 2:
        switch (theirs) {
        case 0:
        case 2:
            return false;
        case 1:
            return true;
        }
        return false;
    }
    return false;
}

// Distance used to rank targets; nearby players count as closer still.
static inline float TargetDistance(CAIObject* self, CAIObject* target) {
    float d = self->GetDistanceXYZReal(target);
    if (target->IsPlayer() && d < 10.0f) {
        d *= 0.5f;
    }
    return d;
}

// Picks the closest visible living enemy. The current target is kept unless
// another is at least 2 units closer.
CAIObject* CAISoldierObject::ChooseTarget(CAIObject* list, float sightRange) {
    CAIObject* best = NULL;
    float bestDist = 0.0f;
    CAIObject* current = m_pTarget;
    if (current != NULL) {
        best = current;
        bestDist = TargetDistance(this, current) - 2.0f;
#ifdef FRONTLINE_NATIVE
        // Rewritten: a dead target, or one long out of sight, loses its
        // preference to any target in sight.
        if (NativeAIOn() && !NativeAIKeepTarget(this, current)) {
            bestDist = 1e30f;
        }
#endif
    }
    for (CAIObject* obj = list; obj != NULL; obj = obj->m_pNext) {
        if (obj == this || obj == best) {
            continue;
        }
        if (!IsEnemyTeam(field_0x14, obj->field_0x14) || (int)obj->m_status.m_vars[AISTATUS_HEALTH] <= 0) {
            continue;
        }
        float range = sightRange;
        if (obj->m_field_0x730) {
            range = sightRange * 0.5f;
        }
        if (obj->IsCrouching()) {
            range *= 0.75f;
        }
        if (IsObjectInLineOfSight(obj->m_position, range, obj->IsCrouching())) {
            float d = TargetDistance(this, obj);
            if (best == NULL || d < bestDist) {
                best = obj;
                bestDist = d;
            }
        }
    }
    if (best != NULL) {
        SetTarget(best);
        return best;
    }
    SetTarget(NULL);
    return NULL;
}

void CAISoldierObject::SetUpperLocation(CAIFilterRealPosition& pos) {
    m_upperLocation.field_0x0 = pos.field_0x0;
    m_upperLocation.field_0x4 = pos.field_0x4;
    m_upperLocation.field_0x8 = pos.field_0x8;
}
