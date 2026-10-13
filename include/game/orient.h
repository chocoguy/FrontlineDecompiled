// Fixed-point orientation helpers. Angles are 24-bit (0x1000000 = full turn);
// lerps take the short way around.
#ifndef _GAME_ORIENT_H_
#define _GAME_ORIENT_H_

#include "types.h"

// Three angles (x, y, z).
struct Orient3_t {
    /* 0x0 */ long x;
    /* 0x4 */ long y;
    /* 0x8 */ long z;
};

extern "C" {
// *out = b + (a - b) * t
void OrientLerpAngle(long* out, long a, long b, float t);  // 0x800B8418
void Orient3Lerp(Orient3_t* out, const Orient3_t* a, const Orient3_t* b, float t);  // 0x800B848C
}

#endif
