// Minimal MSL math.h declarations, added as decompiled code needs them.
#ifndef _MSL_MATH_H_
#define _MSL_MATH_H_

#ifdef __cplusplus
extern "C" {
#endif

double sin(double x);
double cos(double x);
float cosf(float x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sqrt(double x);
double floor(double x);
double ceil(double x);
double fmod(double x, double y);
double pow(double x, double y);

#ifdef __cplusplus
}
#endif

inline double fabs(double x) { return __fabs(x); }
inline float fabsf(float x) { return (float)__fabs(x); }

// Inline sqrtf as in the game's math.h: three Newton-Raphson steps on frsqrte.
// The game's copy uses plain literals (0.5, 3.0) rather than MSL's statics.
inline float sqrtf(float x) {
    volatile float y;
    if (x > 0.0f) {
        double guess = __frsqrte((double)x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        y = (float)(x * guess);
        return y;
    }
    return x;
}

#endif
