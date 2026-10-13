#include "game/AIPlayerObject.h"

#include "game/player.h"
#include "game/scene.h"

CAIPlayerObject::CAIPlayerObject(void* gameObject) {
    m_pFilter = (CAIFilterObject*)gameObject;
}

CAIPlayerObject::~CAIPlayerObject() {}

void CAIPlayerObject::PreUpdate() {}

void CAIPlayerObject::Update(float dt) {
    m_status.m_vars[AISTATUS_HEALTH] = AIHealthLevel(m_health, m_maxHealth);
    m_status.m_vars[AISTATUS_THREAT] = 4;
    m_status.m_vars[AISTATUS_VAR4] = 1;
}

void CAIPlayerObject::Init() {
    CAIObject::Init();
    m_status.m_vars[AISTATUS_HEALTH] = AIHealthLevel(m_health, m_maxHealth);
    m_status.m_vars[AISTATUS_VAR1] = field_0x15;
    m_status.m_vars[AISTATUS_VAR2] = field_0x214;
    m_status.m_vars[AISTATUS_THREAT] = 4;
    m_status.m_vars[AISTATUS_VAR4] = 1;
    m_status.m_vars[AISTATUS_TEAM] = field_0x14;
    m_status.m_vars[AISTATUS_ATTACKERS] = 0;
}

bool CAIPlayerObject::IsCrouching() const {
    return g_scene.GetPlayer(0)->IsPlayerCrouching();
}
