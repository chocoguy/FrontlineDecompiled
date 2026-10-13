#include "game/AIPlayerDoodad.h"

#include "game/AIFilter.h"
#include "game/AIPlayerObject.h"

CAIPlayerDoodad::CAIPlayerDoodad() {
    m_pFilter = NULL;
}

CAIPlayerDoodad::~CAIPlayerDoodad() {}

void CAIPlayerDoodad::Init(CPlayerObject* player) {
    if (m_pFilter == NULL) {
        m_pFilter = new CAIFilterPlayerObject;
    }
    m_pFilter->LinkGameObject(this);
    m_pPlayer = player;
    CAIDoodad::Init();
    CAIFilterPlayerObject* filter = (CAIFilterPlayerObject*)m_pFilter;
    filter->SetOneTimeVars();
    filter->UpdateManualUpdateVars();
    ((CAIPlayerObject*)filter->m_pAIObject)->Init();
}

void CAIPlayerDoodad::CleanUp() {
    delete m_pFilter;
}

void CAIPlayerDoodad::PreUpdate() {
    ((CAIFilterPlayerObject*)m_pFilter)->PreUpdate();
}

void CAIPlayerDoodad::Update(float dt) {
    ((CAIFilterPlayerObject*)m_pFilter)->Update(dt);
}
