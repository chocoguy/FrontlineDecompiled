#include "fdlibm.h"

// Returns x with the sign bit of y.
double copysign(double x, double y) {
    __HI(x) = (__HI(x) & 0x7FFFFFFF) | (__HI(y) & 0x80000000);
    return x;
}
