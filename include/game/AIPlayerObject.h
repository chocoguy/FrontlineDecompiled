// CAIPlayerObject: the AI system's view of the player. Hand-maintained.
#ifndef _GAME_AIPLAYEROBJECT_H_
#define _GAME_AIPLAYEROBJECT_H_

#include "types.h"
#include "game/AIObject.h"

// size 0x740; vtable 0x8018352C
class CAIPlayerObject : public CAIObject {
public:
    /* 0x734 */ u8 unk_0x734[0x4];
    /* 0x738 */ f32 m_health;
    /* 0x73C */ f32 m_maxHealth;

    CAIPlayerObject(void* gameObject);  // 0x8004F2D0
    virtual ~CAIPlayerObject();  // 0x8004F270
    virtual void PreUpdate();  // 0x8004F26C
    virtual void Update(float dt);  // 0x8004F228
    void Init();  // 0x8004F1A4
    virtual bool IsCrouching() const;  // 0x8004F174
    virtual bool IsPlayer() const { return true; }  // 0x8004F16C
};

#endif
