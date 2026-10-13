// Small math helper templates. Hand-maintained.
#ifndef _GAME_MATHFUN_H_
#define _GAME_MATHFUN_H_

template <typename T>
T MathFunClamp(T value, T lo, T hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

#include "types.h"

class CVector3;

s64 MathFunRandomI64(s64 lo, s64 hi);  // 0x80092DD4
s64 MathFunGetRandomPercent();  // 0..99
bool MathFunTestPercent(s64 roll, s64 percent);  // roll < percent
bool MathFunCloseToZero(float value, float tolerance);  // -tolerance <= value <= tolerance
// Signed pan (about z) / tilt angle from `from` to `to` relative to the facing.
float MathFunGetTiltAngleDiffNoRoll(CVector3* from, CVector3* to, CVector3* forward, CVector3* up);
float MathFunGetPanAngleDiffNoRoll(CVector3* from, CVector3* to, CVector3* forward);
float MathFunNormalizeAngleNegativePiToPi(float angle);
void MathFunRotateAboutZ(float* x, float* y, float angle);
void MathFunRotateAboutY(float* x, float* z, float angle);
float MathFunAtan2F(float y, float x);
void MathFunSRandom(unsigned int seed);  // 0x80092E90
u32 MathFunRandomUI32Raw();  // 0..0x7FFFFFFF
extern bool g_bMathFunRandomSeeded;
float MathFunRandomRealSigned(float lo, float hi);  // 0x80092CBC
float MathFunRandomReal(float lo, float hi);  // 0x80092D60

#endif
