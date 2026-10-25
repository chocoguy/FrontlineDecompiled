// CAIDoodad: base of the per-object AI adapters (player, soldier, tank) that
// connect a game object to its CAIFilterObject.
// Hand-maintained.
#ifndef _GAME_AIDOODAD_H_
#define _GAME_AIDOODAD_H_

#include "types.h"
#include "game_types.h"
#ifdef FRONTLINE_NATIVE
#include <stddef.h>
#endif

class CAIFilterObject;
class CStaticObject;
class ISceneNode;

// size 0x48; vtable 0x80183620
class CAIDoodad {
public:
    virtual ~CAIDoodad();  // 0x800590F8
    virtual ISceneNode* GetSceneNode() const { return NULL; }  // 0x800590EC
    virtual void FireAtCurrentTarget(float) {}  // 0x80032BB4
    virtual void FireMMGAtCurrentTarget(CStaticObject*) {}  // 0x80032AFC
    /* 0x04 */ CAIFilterObject* m_pFilter;
    // Bit 0 of each: the unit fired (this frame / last frame). A new shot
    // raises BS event 0x7C on the soldier's script object.
    /* 0x08 */ u32 m_fireFlags;
    /* 0x0C */ u32 m_prevFireFlags;
    // Four pointers to the inline buffers below (set by the constructor).
    // Scripts read this block by offset as the soldier's match list struct
    // (BIFunc_AIGetSoldierMatchListStruct), so the pointers are 32-bit.
    /* 0x10 */ PTR32(void) m_pBuffer0;
    /* 0x14 */ PTR32(void) m_pBuffer1;
    /* 0x18 */ PTR32(void) m_pBuffer2;
    /* 0x1C */ PTR32(void) m_pBuffer3;
    /* 0x20 */ u8 m_buffer0[0xC];
    /* 0x2C */ u8 m_buffer1[0xC];
    /* 0x38 */ u8 m_buffer2[0xC];
    /* 0x44 */ u8 m_buffer3[0x4];
    void Init();  // 0x800590F4
    CAIDoodad();  // 0x80059140
};
#ifdef FRONTLINE_NATIVE
static_assert(offsetof(CAIDoodad, m_buffer3) - offsetof(CAIDoodad, m_pBuffer0) == 0x34,
              "the match list struct keeps its console layout");
#endif

#endif
