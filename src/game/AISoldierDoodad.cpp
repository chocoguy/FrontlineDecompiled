#include "game/AISoldierDoodad.h"

#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/AISoldierObject.h"
#include "game/Soldier_object.h"
#include "game/bsobject.h"
#include "game/matrix.h"
#include "game/weapon.h"

// BS (script) event raised when a soldier starts firing.
#define BSEVENT_SOLDIER_FIRED 0x7C

CAISoldierDoodad::CAISoldierDoodad() {}

CAISoldierDoodad::~CAISoldierDoodad() {
    delete m_pFilter;
}

// The filter is placed just past the soldier object: objects are created in
// fixed-size factory slots, and soldiers use the slack for it (players, which
// derive from CSoldierObject, use that space for their own fields instead).
#define SOLDIER_AI_FILTER_OFFSET 0x44B0

void CAISoldierDoodad::Init(CSoldierObject* soldier) {
    m_pFilter = new ((u8*)soldier + SOLDIER_AI_FILTER_OFFSET) CAIFilterSoldierObject;
    m_pFilter->LinkGameObject(this);
    m_pSoldier = soldier;
    CAIDoodad::Init();
    CAIFilterSoldierObject* filter = (CAIFilterSoldierObject*)m_pFilter;
    filter->SetOneTimeVars();
    filter->UpdateManualUpdateVars();
    ((CAISoldierObject*)filter->m_pAIObject)->Init();
}

void CAISoldierDoodad::CleanUp() {
    delete m_pFilter;
}

void CAISoldierDoodad::PreUpdate() {
    ((CAIFilterSoldierObject*)m_pFilter)->PreUpdate();
}

void CAISoldierDoodad::Update(float dt) {
    CAIFilterSoldierObject* filter = (CAIFilterSoldierObject*)m_pFilter;
    filter->Update(dt);
    if ((m_fireFlags & 1) && !(m_prevFireFlags & 1)) {
        BSObjectTriggerEvent(filter->m_pBSObject, BSEVENT_SOLDIER_FIRED, NULL, NULL, false);
    }
}

// An AI soldier never runs dry mid-burst: a reloading weapon is finished
// and topped up before firing.
static inline void FinishReload(CSoldierObject* soldier) {
    bool reloading = false;
    CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    if (weapon != NULL) {
        reloading = weapon->m_bReloading;
    }
    if (reloading) {
        CWeapon** slot = &soldier->m_pWeapons[soldier->m_curWeapon];
        if (*slot != NULL) {
            (*slot)->DoneReloading();
            (*slot)->AddAmo(20);
        }
    }
}

void CAISoldierDoodad::FireAtCurrentTarget(float accuracy) {
    CAIFilterObject* filter = m_pFilter;
    CAIFilterRealPosition target;
    filter->GetTargetGuessPosition(target);
    FinishReload(m_pSoldier);

    CMatrix emitter;
    m_pSoldier->GetBulletEmitterMatrix(emitter);
    CVector3 from = *(CVector3*)emitter.m[3];
    CVector3 dir(target.field_0x0 - from.x, target.field_0x4 - from.y, target.field_0x8 - from.z);
    if (m_pSoldier->FireWeapon(m_pSoldier->m_curWeapon, from, dir, accuracy)) {
        filter->m_pAIObject->m_flags |= 1;
    }
}

void CAISoldierDoodad::FireMMGAtCurrentTarget(CStaticObject* gun) {
    CAIFilterObject* filter = m_pFilter;
    CAIFilterRealPosition target;
    filter->GetTargetGuessPosition(target);
    FinishReload(m_pSoldier);

    if (m_pSoldier->FireMMG(*(CVector3*)&target, gun)) {
        filter->m_pAIObject->m_flags |= 1;
    }
}
