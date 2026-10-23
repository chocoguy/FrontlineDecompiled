#include "ea/snd/smix.h"
#include <string.h>

void SFILTER_echorestore(void* self);
int SFILTER_echo(void* self, int frames, void* in, void* out, int flag);

// params: feedback (0..254, << 1), delay ms << 8, rate << 8.
void SFILTER_modifyECHO(ECHOSTATE* s, int* params) {
    int fb = params[0] >> 1;
    int ms = params[1] >> 8;
    int rate = params[2] >> 8;
    if (s->mem != NULL) {
        SNDMEMI_free(s->mem);
        s->mem = NULL;
        s->buf = NULL;
    }
    s->fb = 0.007874016f * fb;
    s->negfb = -s->fb;
    s->len = SNDI_findmult16(ms, rate);
    u32 bytes = s->len * 4;
    s->mem = SNDMEMI_allocz(bytes + 16);
    s->buf = (f32*)(((uptr)(s->mem) + 15) & ~15);
    memset(s->buf, 0, bytes);
    s->pos = 0;
}

int SFILTER_createECHO(ECHOSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = (void (*)(SFILTER*))SFILTER_echorestore;
    s->f.process = SFILTER_echo;
    s->mem = NULL;
    s->buf = NULL;
    return 0;
}

void SFILTER_echorestore(void* self) {
    ECHOSTATE* s = (ECHOSTATE*)self;
    if (s->mem != NULL) {
        SNDMEMI_free(s->mem);
        s->mem = NULL;
        s->buf = NULL;
    }
}

// Processes in steps of 4 samples (frame counts are multiples of 4).
int SFILTER_echo(void* self, int frames, void* in, void* out, int flag) {
    ECHOSTATE* s = (ECHOSTATE*)self;
    if (s->f.next != NULL) {
        int got = s->f.next->process(s->f.next, frames, out, in, s->f.port);
        if (got <= 0) {
            return got;
        }
    }
    int left = frames;
    f32* src = (f32*)in;
    f32* dest = (f32*)out;
    while (left > 0) {
        if (s->pos >= s->len) {
            s->pos = 0;
        }
        int n = s->len - s->pos;
        if (n > left) {
            n = left;
        }
        f32* end = dest + n;
        f32* o = dest;
        f32* x = src;
        f32* d = s->buf + s->pos;
        for (; o < end; o += 4, x += 4, d += 4) {
            f32 d0 = d[0];
            f32 t0 = 1e-20f + (x[0] + d0 * s->fb);
            f32 t1 = 1e-20f + (x[1] + d[1] * s->fb);
            f32 t2 = 1e-20f + (x[2] + d[2] * s->fb);
            f32 t3 = 1e-20f + (x[3] + d[3] * s->fb);
            o[0] = d0 + t0 * s->negfb;
            o[1] = d[1] + t1 * s->negfb;
            o[2] = d[2] + t2 * s->negfb;
            o[3] = d[3] + t3 * s->negfb;
            d[0] = t0;
            d[1] = t1;
            d[2] = t2;
            d[3] = t3;
        }
        left -= n;
        src += n;
        dest += n;
        s->pos += n;
    }
    return frames;
}
