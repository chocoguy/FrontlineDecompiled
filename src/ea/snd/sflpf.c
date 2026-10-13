#include "ea/snd/smix.h"

int SFILTER_lpfRC(void* self, int frames, void* in, void* out, int flag);

// params: cutoff << 7 (scaled to rate << 8), rate << 8, gain << 8.
void SFILTER_modifyLPFRC(LPFRCSTATE* s, int* params) {
    s->b = 2.0f * params[0] / params[1];
    s->a = 1.0f - s->b;
    s->b *= 0.00390625f * params[2];
    s->a *= 0.00390625f * params[2];
}

int SFILTER_createLPFRC(LPFRCSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = NULL;
    s->f.process = SFILTER_lpfRC;
    s->y = 0.0f;
    return 0;
}

int SFILTER_lpfRC(void* self, int frames, void* in, void* out, int flag) {
    LPFRCSTATE* s = (LPFRCSTATE*)self;
    if (s->f.next != NULL) {
        int got = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (got <= 0) {
            return got;
        }
    }
    f32* src = (f32*)in;
    f32* dest = (f32*)out;
    for (int i = 0; i < frames; i++) {
        s->y = s->y * s->a + s->b * *src++;
        *dest++ = s->y;
    }
    return frames;
}
