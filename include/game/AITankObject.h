// CAITankObject: AI for tanks. Hand-maintained.
#ifndef _GAME_AITANKOBJECT_H_
#define _GAME_AITANKOBJECT_H_

#include "types.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/AIStatus.h"

// size 0x7A8; vtable 0x801835DC
class CAITankObject : public CAIObject {
public:
    /* 0x734 */ u8 unk_0x734[0x4];
    /* 0x738 */ f32 m_health;
    /* 0x73C */ f32 m_maxHealth;
    /* 0x740 */ int m_targetMode;  // aifilter_target_mode
    /* 0x744 */ aistatus_match m_targetMatches[4];
    /* 0x774 */ int m_numTargetMatches;
    /* 0x778 */ CAIFilterRealPosition m_positions[3];

    CAITankObject(void* gameObject);  // 0x80052358
    virtual ~CAITankObject();  // 0x80052314
    // Lives in a buffer inside its owner (see CAITankDoodad::Init).
    void* operator new(unsigned long size, void* buffer);  // 0x8005230C
    void operator delete(void*) {}
    virtual void PreUpdate();  // 0x80052308
    virtual void Update(float dt);  // 0x80052200
    void Init();  // 0x80052174
    void ResetTargetMatchList(aifilter_target_mode mode);  // 0x80051FB0
    void SetupTargetMatchList(aifilter_target_mode mode, aistatus_match* list, int count);  // 0x80051EF8
    CAIObject* ChooseTarget(CAIObject* list, float);  // 0x80051E8C
};

#endif
