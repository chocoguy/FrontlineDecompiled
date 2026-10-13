#include "ea/snd/smix.h"

int SFILTER_lpfFIR8(void* self, int frames, void* in, void* out, int flag);

// params: cutoff << 8, rate << 8.
void SFILTER_modifyLPFFIR8(LPFSTATE* s, int* params) {
    f32 f = 2.0f * (params[0] >> 8);
    s->fir.f2 = f / (params[1] >> 8);
    calcFIRCoeffs(&s->fir, 2);
}

int SFILTER_createLPFFIR8(LPFSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = NULL;
    s->f.process = SFILTER_lpfFIR8;
    SNDI_fir8init(&s->fir);
    return 0;
}

// Pulls the input from upstream into in, then filters it to out.
int SFILTER_lpfFIR8(void* self, int frames, void* in, void* out, int flag) {
    LPFSTATE* s = (LPFSTATE*)self;
    if (s->f.next != NULL) {
        int got = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (got <= 0) {
            return got;
        }
    }
    SNDI_fir8(&s->fir, frames, in, out);
    return frames;
}
