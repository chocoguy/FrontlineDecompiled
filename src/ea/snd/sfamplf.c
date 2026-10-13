#include "ea/snd/smix.h"

int SFILTER_amplf(void* self, int frames, void* in, void* out, int flag);

// params[0]: gain << 8.
void SFILTER_modifyAMPLF(AMPLFSTATE* s, int* params) {
    s->gain = 0.00390625f * *params;
}

int SFILTER_createAMPLF(AMPLFSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = NULL;
    s->f.process = SFILTER_amplf;
    s->gain = 0.0f;
    return 0;
}

int SFILTER_amplf(void* self, int frames, void* in, void* out, int flag) {
    AMPLFSTATE* s = (AMPLFSTATE*)self;
    if (s->f.next != NULL) {
        int got = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (got <= 0) {
            return got;
        }
    }
    f32* src = (f32*)in;
    f32* dest = (f32*)out;
    for (int i = 0; i < frames; i++) {
        *dest++ = s->gain * *src++;
    }
    return frames;
}
