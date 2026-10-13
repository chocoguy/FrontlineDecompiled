#include "game/MathFun.h"

#include <math.h>
#include <stdlib.h>
#include "ea/sys/random.h"
#include "game/vector.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define PI 3.1415927f
#define HALF_PI 1.5707964f
#define TWO_PI 6.2831855f
// 1 / 2^31
#define RANDOM_SCALE 4.656613e-10f

bool g_bMathFunRandomSeeded;

// The game's atan2 convention: the angle of (y, x) measured the other way
// round (atan(-y / x) in the right half-plane).
float MathFunAtan2F(float y, float x) {
    if (x > 0.0f) {
        return atan(-y / x);
    }
    if (x < 0.0f) {
        if (y < 0.0f) {
            return PI - (float)atan(y / x);
        }
        return -PI - (float)atan(y / x);
    }
    if (y <= 0.0f) {
        return HALF_PI;
    }
    return -HALF_PI;
}

void MathFunRotateAboutY(float* x, float* z, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    float tx = *x;
    float tz = *z;
    *z = tz * c - tx * s;
    *x = tz * s + tx * c;
}

void MathFunRotateAboutZ(float* x, float* y, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    float tx = *x;
    float ty = *y;
    *x = tx * c - ty * s;
    *y = tx * s + ty * c;
}

float MathFunNormalizeAngleNegativePiToPi(float angle) {
    angle = angle - PI * (float)((int)(angle / TWO_PI) * 2);
    if (angle > PI) {
        return angle - TWO_PI;
    }
    if (angle < -PI) {
        angle += TWO_PI;
    }
    return angle;
}

// Note: both angle-difference helpers leave `to` relative to `from` (and
// the tilt one also rotates `to` and `up`).
float MathFunGetPanAngleDiffNoRoll(CVector3* from, CVector3* to, CVector3* forward) {
    to->x -= from->x;
    to->y -= from->y;
    to->z -= from->z;
    float facing = MathFunAtan2F(-forward->y, forward->x);
    float pan = MathFunAtan2F(to->x, to->y);
    return MathFunNormalizeAngleNegativePiToPi(pan - facing);
}

float MathFunGetTiltAngleDiffNoRoll(CVector3* from, CVector3* to, CVector3* forward, CVector3* up) {
    float facing = MathFunAtan2F(-forward->y, forward->x);
    MathFunRotateAboutZ(&up->x, &up->y, -facing);
    to->x -= from->x;
    to->y -= from->y;
    to->z -= from->z;
    float pan = MathFunAtan2F(to->x, to->y);
    MathFunRotateAboutZ(&to->x, &to->y, -pan);
    float upTilt = MathFunAtan2F(-up->z, up->y);
    float tilt = MathFunAtan2F(-to->z, to->y);
    return MathFunNormalizeAngleNegativePiToPi(tilt - upTilt);
}

bool MathFunCloseToZero(float value, float tolerance) {
    if (value > tolerance) {
        return false;
    }
    return !(value < -tolerance);
}

void MathFunSRandom(unsigned int seed) {
    srand(seed);
    g_bMathFunRandomSeeded = true;
}

u32 MathFunRandomUI32Raw() {
    return random() & 0x7FFFFFFF;
}

// lo..hi inclusive.
s64 MathFunRandomI64(s64 lo, s64 hi) {
    return lo + (((hi - lo + 1) * (s64)MathFunRandomUI32Raw()) >> 31);
}

float MathFunRandomReal(float lo, float hi) {
    return (hi - lo) * (RANDOM_SCALE * (float)MathFunRandomUI32Raw()) + lo;
}

// lo..hi with a random sign.
float MathFunRandomRealSigned(float lo, float hi) {
    float value = MathFunRandomReal(lo, hi);
    return value * (float)(int)((random() & 2) - 1);
}

s64 MathFunGetRandomPercent() {
    return (100 * (s64)MathFunRandomUI32Raw()) >> 31;
}

bool MathFunTestPercent(s64 roll, s64 percent) {
    return roll < percent;
}
