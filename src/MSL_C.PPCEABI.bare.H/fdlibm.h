// fdlibm (Sun's freely distributable libm) as built into MSL: big-endian
// word access and _IEEE_ mode (the w_ wrappers call straight through).
#ifndef _MSL_FDLIBM_H_
#define _MSL_FDLIBM_H_

#include <math.h>

#define __HI(x) *(int*)&x
#define __LO(x) *(1 + (int*)&x)

#ifdef __cplusplus
extern "C" {
#endif

double copysign(double x, double y);
double ldexp(double x, int n);
double frexp(double x, int* eptr);
double modf(double x, double* iptr);

double __ieee754_acos(double x);
double __ieee754_asin(double x);
double __ieee754_atan2(double y, double x);
double __ieee754_fmod(double x, double y);
double __ieee754_pow(double x, double y);
int __ieee754_rem_pio2(double x, double* y);

double __kernel_sin(double x, double y, int iy);
double __kernel_cos(double x, double y);
double __kernel_tan(double x, double y, int iy);
int __kernel_rem_pio2(double* x, double* y, int e0, int nx, int prec, const int* ipio2);

#ifdef __cplusplus
}
#endif

// MSL's fpclassify for doubles.
#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5

static inline int __fpclassifyd(double x) {
    switch (__HI(x) & 0x7FF00000) {
    case 0x7FF00000:
        if ((__HI(x) & 0x000FFFFF) || __LO(x)) {
            return FP_NAN;
        } else {
            return FP_INFINITE;
        }
        break;
    case 0:
        if ((__HI(x) & 0x000FFFFF) || __LO(x)) {
            return FP_SUBNORMAL;
        } else {
            return FP_ZERO;
        }
        break;
    }
    return FP_NORMAL;
}

#define isfinite(x) (__fpclassifyd(x) > FP_INFINITE)

// errno.c
extern int errno;
#define EDOM 33
#define ERANGE 34

// float.c
extern unsigned long __float_nan[];
extern unsigned long __float_huge[];
#define NAN (*(float*)__float_nan)
#define INFINITY (*(float*)__float_huge)

// MSL's inline sqrt: four Newton-Raphson steps on frsqrte.
static inline double __msl_sqrt(double x) {
    if (x > 0.0) {
        double guess = __frsqrte(x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        return x * guess;
    } else if (x == 0) {
        return 0;
    } else if (x) {
        return NAN;
    }
    return INFINITY;
}
#define sqrt __msl_sqrt

#endif
