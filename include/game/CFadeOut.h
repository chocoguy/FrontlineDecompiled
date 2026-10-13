// The multiplayer end-of-game fade-out. Hand-maintained.
#ifndef _GAME_CFADEOUT_H_
#define _GAME_CFADEOUT_H_

#include "types.h"
#include "game/fader.h"

// size 0x14; vtable 0x80181AD0
class CFadeOut : public CFaderControl {
public:
    CFadeOut(CColor from, CColor to, float duration) : CFaderControl(from, to, duration) {}
};

#endif
