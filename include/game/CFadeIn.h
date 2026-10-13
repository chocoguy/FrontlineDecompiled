// The level fade-in (g_FadeIn). Hand-maintained.
#ifndef _GAME_CFADEIN_H_
#define _GAME_CFADEIN_H_

#include "types.h"
#include "game/fader.h"

// size 0x14; vtable 0x80181AE8
class CFadeIn : public CFaderControl {
public:
    CFadeIn(CColor from, CColor to, float duration) : CFaderControl(from, to, duration) {}
};

#endif
