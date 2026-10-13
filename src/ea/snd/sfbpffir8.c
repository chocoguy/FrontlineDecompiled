#include "ea/snd/smix.h"

int SFILTER_bpfFIR8(void* self, int frames, void* in, void* out, int flag);

// params: low << 8, high << 8, rate << 8.
void SFILTER_modifyBPFFIR8(BPFSTATE* s, int* params) {
    int high = params[1] >> 7;
    int rate = params[2] >> 8;
    f32 f1 = (f32)(params[0] >> 7) / rate;
    f32 f2 = high;
    s->fir.f1 = f1;
    s->fir.f2 = f2 / rate;
    calcFIRCoeffs(&s->fir, 4);
}

int SFILTER_createBPFFIR8(BPFSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = NULL;
    s->f.process = SFILTER_bpfFIR8;
    SNDI_fir8init(&s->fir);
    return 0;
}

// Pulls the input from upstream into in, then filters it to out.
int SFILTER_bpfFIR8(void* self, int frames, void* in, void* out, int flag) {
    BPFSTATE* s = (BPFSTATE*)self;
    if (s->f.next != NULL) {
        frames = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (frames <= 0) {
            return frames;
        }
    }
    SNDI_fir8(&s->fir, frames, in, out);
    return frames;
}
