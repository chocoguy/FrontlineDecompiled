#include "game/AITankDoodad.h"

#include "game/AIFilter.h"
#include "game/AITankObject.h"
#include "game/Tank_object.h"

CAITankDoodad::CAITankDoodad() {}

CAITankDoodad::~CAITankDoodad() {
    delete m_pFilter;
    m_pFilter = NULL;
}

// The filter is placed in the tank object's factory slot, past the fields
// known so far (see CAISoldierDoodad::Init).
#define TANK_AI_FILTER_OFFSET 0x830

void CAITankDoodad::Init(CTankObject* tank) {
    m_pFilter = new ((u8*)tank + TANK_AI_FILTER_OFFSET) CAIFilterTankObject;
    m_pFilter->LinkGameObject(this);
    m_pTank = tank;
    CAIDoodad::Init();
    CAIFilterTankObject* filter = (CAIFilterTankObject*)m_pFilter;
    filter->SetOneTimeVars();
    filter->UpdateManualUpdateVars();
    ((CAITankObject*)filter->m_pAIObject)->Init();
}

void CAITankDoodad::CleanUp() {
    delete m_pFilter;
    m_pFilter = NULL;
}

void CAITankDoodad::PreUpdate() {
    ((CAIFilterTankObject*)m_pFilter)->PreUpdate();
}

void CAITankDoodad::Update(float dt) {
    ((CAIFilterTankObject*)m_pFilter)->Update(dt);
}
