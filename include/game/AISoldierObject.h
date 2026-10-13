// CAISoldierObject: AI for infantry. Hand-maintained.
#ifndef _GAME_AISOLDIEROBJECT_H_
#define _GAME_AISOLDIEROBJECT_H_

#include "types.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/AIStatus.h"

// size 0x7C8; vtable 0x80183554
class CAISoldierObject : public CAIObject {
public:
    /* 0x734 */ u8 unk_0x734[0x4];
    /* 0x738 */ f32 m_headPosition[3];  // CSoldierObject 0x3FA0
    /* 0x744 */ u8 unk_0x744[0x4];
    /* 0x748 */ CAIFilterRealPosition m_upperLocation;
    /* 0x758 */ CAIFilterRealPosition m_position2;
    /* 0x768 */ f32 m_health;
    /* 0x76C */ f32 m_maxHealth;
    /* 0x770 */ int m_targetMode;  // aifilter_target_mode
    /* 0x774 */ aistatus_match m_targetMatches[4];
    /* 0x7A4 */ int m_numTargetMatches;
    /* 0x7A8 */ u8 unk_0x7A8[0x20];

    void* operator new(unsigned long size, void* buffer);  // 0x8004FA8C
    void operator delete(void*) {}
    CAISoldierObject(void* gameObject);  // 0x8004FA1C
    virtual ~CAISoldierObject();  // 0x8004F9D8
    virtual void PreUpdate();  // 0x8004F9D4
    virtual void Update(float dt);  // 0x8004F8CC
    void Init();  // 0x8004F830
    void ResetTargetMatchList(aifilter_target_mode mode);  // 0x8004F6A0
    void SetupTargetMatchList(aifilter_target_mode mode, aistatus_match* list, int count);  // 0x8004F5E4
    CAIObject* ChooseTarget(CAIObject* list, float sightRange);  // 0x8004F338
    void SetUpperLocation(CAIFilterRealPosition& pos);  // 0x8004F31C
};

#endif
