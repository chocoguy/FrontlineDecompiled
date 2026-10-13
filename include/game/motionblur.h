// CMotionBlur: full-screen motion blur, drawn by a render bin registered with
// the render list. Hand-maintained.
#ifndef _GAME_MOTIONBLUR_H_
#define _GAME_MOTIONBLUR_H_

#include "types.h"
#include "game/renderbin.h"

class CRenderList;

// size 0x20; vtable 0x801847E8
class CBlurBin : public CRenderBin {
public:
    CBlurBin() { m_type = 0x71; }
    virtual bool IsUsed();  // 0x8007D2C8
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x8007D2E4
};

class CMotionBlur {
public:
    static void SetAmount(float amount);  // 0x8007D2E8
    static void Init(CRenderList* renderList);  // 0x8007D2F0
};

#endif
