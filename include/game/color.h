// CColor: 8-bit RGBA color. Hand-maintained.
#ifndef _GAME_COLOR_H_
#define _GAME_COLOR_H_

#include "types.h"

struct CColor {
    u8 r, g, b, a;
    CColor() {}
    CColor(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {}
};

#endif
