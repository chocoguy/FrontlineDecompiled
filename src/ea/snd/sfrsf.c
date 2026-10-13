#include "ea/snd/smix.h"

int SFILTER_rsf(void* self, int frames, void* in, void* out, int flag);

// Only the low quality (linear) kernel exists; hq leaves it unset.
void SFILTER_rsfinit(RSFSTATE* s, int quality, int hq) {
    s->f.process = SFILTER_rsf;
    s->f.destroy = NULL;
    s->phase = 0;
    s->primed = 0;
    s->delay = 0;
    if (hq == 0) {
        s->kernel = rsflc;
    }
    s->hist[0] = 0.0f;
    s->hist[1] = 0.0f;
    s->hist[2] = 0.0f;
    s->hist[3] = 0.0f;
}

static inline void clearhist(RSFSTATE* s) {
    if (!s->primed) {
        s->primed = 1;
    }
    s->hist[0] = 0.0f;
    s->hist[1] = 0.0f;
    s->hist[2] = 0.0f;
    s->hist[3] = 0.0f;
}

static inline void sethist(RSFSTATE* s, f32* p) {
    s->hist[0] = p[0];
    s->hist[1] = p[1];
    s->hist[2] = p[2];
    s->hist[3] = p[3];
}

int SFILTER_rsf(void* self, int frames, void* in, void* out, int flag) {
    RSFSTATE* s = (RSFSTATE*)self;
    f32* src = (f32*)in;
    f32* dest = (f32*)out;
    int n = frames;
    int count = frames;
    int i;

    if (s->pitch == 0x10000) {
        // Unity pitch: pass through, delayed by the history.
        if (!s->primed) {
            n += 4 - s->delay;
        } else {
            for (i = 0; i < 4 - s->delay; i++) {
                *dest++ = s->hist[i + s->delay];
            }
        }
        if (s->f.next != NULL) {
            count = s->f.next->process(s->f.next, n, src, dest, s->f.port);
        }
        if (count <= 0) {
            clearhist(s);
            return count;
        }
        f32* p;
        if (!s->primed) {
            s->primed = 1;
            p = dest + (frames - s->delay);
        } else {
            p = dest + (frames - 4);
        }
        sethist(s, p);
        return frames;
    }

    int need = (s->phase + s->pitch * frames) >> 16;
    if (!s->primed) {
        need += 4 - s->delay;
        for (i = 0; i < s->delay; i++) {
            *src++ = 0.0f;
        }
    } else {
        src[0] = s->hist[0];
        src[1] = s->hist[1];
        src[2] = s->hist[2];
        src[3] = s->hist[3];
        src += 4;
    }
    if (need > 0 && s->f.next != NULL) {
        int got = s->f.next->process(s->f.next, need, dest, src, s->f.port);
        if (got <= 0) {
            clearhist(s);
            return got;
        }
    }
    if (s->primed) {
        src -= 4 - s->delay;
    } else {
        s->primed = 1;
    }
    int ipos = 0;
    u32 frac = s->phase << 16;
    s->kernel(frames, src, dest, &ipos, &frac, s->pitch >> 16, s->pitch << 16);
    s->phase = (frac >> 16) | (ipos << 16);
    sethist(s, src + ((s->phase >> 16) - s->delay));
    s->phase &= 0xFFFF;
    return frames;
}

void SFILTER_rsfsetpitch(RSFSTATE* s, int pitch) {
    s->pitch = pitch;
}
