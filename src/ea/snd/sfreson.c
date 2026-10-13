#include "ea/snd/smix.h"

int SFILTER_reson(void* self, int frames, void* in, void* out, int flag);
void resonx87(RESONSTATE* s, int frames, void* in, void* out);

// params: freq << 8, rate << 8, bandwidth << 8, gain << 8.
void SFILTER_modifyRESON(RESONSTATE* s, int* params) {
    int freq = params[0] >> 8;
    int rate = params[1] >> 8;
    int bw = params[2] >> 8;
    f32 r = 1.0f - (3.1415927f * bw) / rate;
    s->rate = rate;
    s->freq = freq;
    f32 gain = params[3];
    s->bw = bw;
    s->r = r;
    f32 r2 = s->r * s->r;
    f32 twor = 2.0f * s->r;
    s->c = (twor / (1.0f + r2)) * SNDI_cos((6.2831855f * freq) / rate);
    s->gain = (1.0f - r2) * (0.00390625f * gain) * SNDI_rootof1plusx(-s->c * s->c);
}

int SFILTER_createRESON(RESONSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = NULL;
    s->f.process = SFILTER_reson;
    s->unk_0x34 = 0.0f;
    s->y1 = 0.0f;
    s->y2 = 0.0f;
    return 0;
}

int SFILTER_reson(void* self, int frames, void* in, void* out, int flag) {
    RESONSTATE* s = (RESONSTATE*)self;
    if (s->f.next != NULL) {
        frames = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (frames <= 0) {
            return frames;
        }
    }
    resonx87(s, frames, in, out);
    return frames;
}

// Passes the input through unchanged when the settings are out of range.
void resonx87(RESONSTATE* s, int frames, void* in, void* out) {
    f32* src = (f32*)in;
    f32* dest = (f32*)out;
    int i;
    if (s->bw < (2.0f * s->rate) / 3.1415927f && s->freq > 0.0f && s->freq < (s->rate >> 1)) {
        for (i = 0; i < frames; i++) {
            *dest = (s->gain * (1e-20f + *src) + s->y1 * (2.0f * s->r * s->c)) - s->y2 * (s->r * s->r);
            src++;
            s->y2 = s->y1;
            s->y1 = *dest;
            dest++;
        }
    } else {
        for (i = 0; i < frames; i++) {
            *dest++ = *src++;
        }
    }
}
