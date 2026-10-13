// Full-screen color fades. Hand-maintained.
#ifndef _GAME_FADER_H_
#define _GAME_FADER_H_

#include "types.h"
#include "game/color.h"
#include "game/renderbin.h"

class CRenderList;

// size 0x20; vtable 0x80184760
class CFaderBin : public CRenderBin {
public:
    CFaderBin() { m_type = 0x72; }
    virtual bool IsUsed();  // 0x8007B970
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x8007B988
};

// A fade from one color to another over a duration (seconds).
// size 0x14; vtable 0x8018477C
class CFaderControl {
public:
    virtual void Update(float dt);  // 0x8007BD00
    /* 0x04 */ CColor m_from;
    /* 0x08 */ CColor m_to;
    /* 0x0C */ f32 m_duration;
    /* 0x10 */ f32 m_time;
    CFaderControl(CColor from, CColor to, float duration);  // 0x8007BEDC
    bool IsFading() const;  // 0x8007BEC0
};

class CFader {
public:
    static void Init(CRenderList* renderList);  // 0x8007C024
    static void Update(float dt);  // 0x8007BFCC
    static void SetColor(CColor color);  // 0x8007BFA4
    // Starts a fade (or with NULL, stops updating it).
    static void SetControl(CFaderControl* control);  // 0x8007BF38
};

#endif
