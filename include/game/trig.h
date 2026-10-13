// Trig helpers. Angles in "LL" units are 2^24 per turn. Hand-maintained.
#ifndef _GAME_TRIG_H_
#define _GAME_TRIG_H_

#include "types.h"
#include "game_types.h"

// Free functions
// atan2(y, x) in LL angle units.
int MathArcTan2(float y, float x);  // 0x80091E64
void MathSinCos(long angle, float* s, float* c);  // 0x80091E9C
float MathCosf(float);  // 0x80091F2C
float MathSinf(float);  // 0x80091F50
void MathLLAngleInit();  // 0x80091F74

#endif
