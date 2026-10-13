#include "ea/snd/sndi.h"

// Taylor series to x^12, after wrapping x below 2 pi.
float SNDI_cos(float x) {
    while (x > 6.2831855f) {
        x -= 6.2831855f;
    }
    float x2 = x * x;
    float x4 = x2 * x2;
    float x6 = x2 * x4;
    float x8 = x4 * x4;
    float x10 = x2 * x8;
    float x12 = x6 * x6;
    return 1.0f - 0.5f * x2 + 0.041666668f * x4 - 0.0013888889f * x6 + 0.000024801588f * x8 -
           0.0000002755732f * x10 + 0.0000000020876758f * x12;
}
