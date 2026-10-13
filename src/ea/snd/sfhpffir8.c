#include "ea/snd/smix.h"

int SFILTER_hpfFIR8(void* self, int frames, void* in, void* out, int flag);

// params: cutoff << 8, rate << 8.
void SFILTER_modifyHPFFIR8(HPFSTATE* s, int* params) {
    f32 f = params[0] >> 7;
    s->fir.f1 = f / (params[1] >> 8);
    calcFIRCoeffs(&s->fir, 3);
}

int SFILTER_createHPFFIR8(HPFSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = NULL;
    s->f.process = SFILTER_hpfFIR8;
    SNDI_fir8init(&s->fir);
    return 0;
}

// Pulls the input from upstream into in, then filters it to out.
int SFILTER_hpfFIR8(void* self, int frames, void* in, void* out, int flag) {
    HPFSTATE* s = (HPFSTATE*)self;
    if (s->f.next != NULL) {
        frames = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (frames <= 0) {
            return frames;
        }
    }
    SNDI_fir8(&s->fir, frames, in, out);
    return frames;
}
