#include "ea/snd/smix.h"

// Linear interpolation resampling. *frac holds the 16-bit fraction in its high half on entry
// and return; the step is stepInt.(stepFrac >> 16).
extern "C" void rsflc(int frames, f32* src, f32* dest, int* ipos, u32* frac, int stepInt, u32 stepFrac) {
    u32 step = (stepFrac >> 16) | (stepInt << 16);
    *frac >>= 16;
    for (int i = 0; i < frames; i++) {
        f32* p = src + *ipos;
        f32 a = p[0];
        *dest++ = a + 0.000015258f * *frac * (p[1] - a);
        *frac += step;
        *ipos += *frac >> 16;
        *frac &= 0xFFFF;
    }
    *frac <<= 16;
}
