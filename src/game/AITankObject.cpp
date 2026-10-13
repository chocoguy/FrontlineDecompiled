#include "game/AITankObject.h"

#include "game/Moh2.h"

CAITankObject::CAITankObject(void* gameObject) {
    m_pFilter = (CAIFilterObject*)gameObject;
    m_targetMode = 3;
}

CAITankObject::~CAITankObject() {}

void* CAITankObject::operator new(unsigned long size, void* buffer) {
    return buffer;
}

void CAITankObject::PreUpdate() {}

void CAITankObject::Update(float dt) {
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

void CAITankObject::Init() {
    CAIObject::Init();
    m_status.m_vars[AISTATUS_HEALTH] = AIHealthLevel(m_health, m_maxHealth);
    m_status.m_vars[AISTATUS_VAR1] = field_0x15;
    m_status.m_vars[AISTATUS_VAR2] = field_0x214;
    m_status.m_vars[AISTATUS_THREAT] = 4;
    m_status.m_vars[AISTATUS_VAR4] = 1;
    m_status.m_vars[AISTATUS_TEAM] = field_0x14;
    m_status.m_vars[AISTATUS_ATTACKERS] = 0;
    m_targetMode = 3;
}

// Status conditions a target must meet, per target mode (op 2 is <, 3 is >).
void CAITankObject::ResetTargetMatchList(aifilter_target_mode) {
    switch (m_targetMode) {
    case 0:
    case 1:
        DebugMsg("WARNING: ResetTargetMatchList called on special target mode.  This is okay for "
                 "now, but not if multiple mode override is implemented");
        break;
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

void CAITankObject::SetupTargetMatchList(aifilter_target_mode, aistatus_match* list, int count) {
    aistatus_match* dst = m_targetMatches;
    for (int i = 0; i < count; i++) {
        *dst++ = *list++;
    }
}

// Targets the first object in the list on another team.
CAIObject* CAITankObject::ChooseTarget(CAIObject* list, float) {
    for (CAIObject* obj = list; obj != NULL; obj = obj->m_pNext) {
        if (obj != this && obj->field_0x14 != field_0x14) {
            SetTarget(obj);
            return obj;
        }
    }
    SetTarget(NULL);
    return NULL;
}
