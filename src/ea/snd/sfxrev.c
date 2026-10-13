#include "ea/snd/smix.h"

// One damped all-pass delay over `frames` samples (the positions do not wrap here).
extern "C" void MIXI_reverbblock(SNDFBSTATE* fb, int frames, f32* in, f32* out) {
    f32* w = fb->buf + fb->wpos;
    f32* r = fb->buf + fb->rpos;
    for (int i = 0; i < frames; i++) {
        f32 x = 1e-30f + *in++;
        f32 y = r[-1] + fb->negg * x;
        r--;
        w[-1] = y * fb->g + x;
        *out++ = y;
        fb->lp = fb->lpb * w[-1] + fb->lp * fb->lpa;
        w[-1] = fb->lp;
        w--;
    }
}
