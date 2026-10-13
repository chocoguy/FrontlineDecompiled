#include "ea/snd/smix.h"

// Output stage: float samples to clamped 16-bit values in 32-bit words.
int SFILTER_ft24_32(void* self, int frames, void* in, void* out, int flag) {
    f32* src = (f32*)in;
    s32* dest = (s32*)out;
    for (int i = 0; i < frames; i++) {
        int v = (int)*src;
        if (v < -0x7FFF) {
            v = -0x7FFF;
        } else if (v > 0x7FFF) {
            v = 0x7FFF;
        }
        *dest = v;
        src++;
        dest++;
    }
    return frames;
}

void SFILTER_ft24_32init(FT24_32STATE* ft) {
    ft->f.process = SFILTER_ft24_32;
    ft->f.destroy = NULL;
}
