#include "ea/snd/smix.h"

void SNDI_fir8(SNDFIRSTATE* fir, int frames, void* in, void* out) {
    f32* src = (f32*)in;
    f32* dest = (f32*)out;
    for (int i = 0; i < frames; i++) {
        *dest = fir->c[0] * (fir->hist[7] + *src) + fir->c[1] * (fir->hist[0] + fir->hist[6]) +
                fir->c[2] * (fir->hist[1] + fir->hist[5]) + fir->c[3] * (fir->hist[2] + fir->hist[4]) +
                fir->c[4] * fir->hist[3];
        dest++;
        fir->hist[7] = fir->hist[6];
        fir->hist[6] = fir->hist[5];
        fir->hist[5] = fir->hist[4];
        fir->hist[4] = fir->hist[3];
        fir->hist[3] = fir->hist[2];
        fir->hist[2] = fir->hist[1];
        fir->hist[1] = fir->hist[0];
        fir->hist[0] = *src++;
    }
}

void SNDI_fir8init(SNDFIRSTATE* fir) {
    fir->hist[0] = 0.0f;
    fir->hist[1] = 0.0f;
    fir->hist[2] = 0.0f;
    fir->hist[3] = 0.0f;
    fir->hist[4] = 0.0f;
    fir->hist[5] = 0.0f;
    fir->hist[6] = 0.0f;
    fir->hist[7] = 0.0f;
}
