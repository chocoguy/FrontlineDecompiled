#include "game/trig.h"

#include <math.h>

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define PI 3.1415927410125732
// LL angle units per radian (2^24 / 2pi).
#define LL_PER_RADIAN 2670176.8f
#define TWO_PI 6.2831855f
#define LL_PER_TURN 16777216.0f

// Taylor series tables for a fixed-point sine. Nothing in the game reads
// them any more (MathSinCos uses the library sin/cos), but they are still
// built at startup.
static f32 _Math_TaylorConst[8];  // 1/3!, 1/5!, ... 1/15!
static f32 _Math_Taylor[9];       // the terms scaled by pi^n, then pi and 1/256
static f32 _Math_InvTaylor[9];    // arcsine series coefficients

int MathArcTan2(float y, float x) {
    return (int)(LL_PER_RADIAN * (float)atan2(y, x));
}

void MathSinCos(long angle, float* s, float* c) {
    float rad = (TWO_PI * (float)angle) / LL_PER_TURN;
    *s = sin(rad);
    *c = cos(rad);
}

float MathCosf(float x) {
    return cos(x);
}

float MathSinf(float x) {
    return sin(x);
}

void MathLLAngleInit() {
    _Math_TaylorConst[0] = 0.16666667f;
    _Math_TaylorConst[1] = 0.008333334f;
    _Math_TaylorConst[2] = 0.0001984127f;
    _Math_TaylorConst[3] = 0.0000027557319f;
    _Math_TaylorConst[4] = 0.000000025052108f;
    _Math_TaylorConst[5] = 0.00000000016059044f;
    _Math_TaylorConst[6] = 0.0000000000007647164f;

    _Math_Taylor[0] = -_Math_TaylorConst[0] * pow(PI, 3.0);
    _Math_Taylor[1] = _Math_TaylorConst[1] * pow(PI, 5.0);
    _Math_Taylor[2] = -_Math_TaylorConst[2] * pow(PI, 7.0);
    _Math_Taylor[3] = _Math_TaylorConst[3] * pow(PI, 9.0);
    _Math_Taylor[4] = -_Math_TaylorConst[4] * pow(PI, 11.0);
    _Math_Taylor[5] = _Math_TaylorConst[5] * pow(PI, 13.0);
    _Math_Taylor[6] = -_Math_TaylorConst[6] * pow(PI, 15.0);
    _Math_Taylor[7] = pow(PI, 1.0);
    _Math_Taylor[8] = 0.00390625f;

    _Math_InvTaylor[0] = 0.16666667f;
    _Math_InvTaylor[1] = 0.025f;
    _Math_InvTaylor[2] = 0.008928572f;
    _Math_InvTaylor[3] = 0.004340278f;
    _Math_InvTaylor[4] = 0.0024857954f;
    _Math_InvTaylor[5] = 0.001577524f;
    _Math_InvTaylor[6] = 0.0010742188f;
    _Math_InvTaylor[7] = 0.00077012007f;
    _Math_InvTaylor[8] = 1.0f;
}
