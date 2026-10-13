#include "ea/snd/sndi.h"

// Taylor series to x^13, after wrapping x below 2 pi.
float SNDI_sin(float x) {
    while (x > 6.2831855f) {
        x -= 6.2831855f;
    }
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x2 * x3;
    float x7 = x2 * x5;
    float x9 = x2 * x7;
    float x11 = x2 * x9;
    float x13 = x2 * x11;
    return x - 0.16666667f * x3 + 0.00833333f * x5 - 0.0001984127f * x7 + 0.0000027557319f * x9 -
           0.000000025052108f * x11 + 0.00000000016059f * x13;
}
