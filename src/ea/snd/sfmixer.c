#include "ea/snd/smix.h"

void SFILTER_mixerrestore(void* self);
int SFILTER_mixer(void* self, int frames, void* in, void* out, int flag);

int SFILTER_createMIX(MIXERSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = (void (*)(SFILTER*))SFILTER_mixerrestore;
    s->f.process = SFILTER_mixer;
    s->buf = NULL;
    s->size = 0;
    return 0;
}

void SFILTER_mixerrestore(void* self) {
    MIXERSTATE* s = (MIXERSTATE*)self;
    if (s->buf != NULL) {
        SNDMEMI_free(s->buf);
    }
}

int SFILTER_mixer(void* self, int frames, void* in, void* out, int flag) {
    MIXERSTATE* s = (MIXERSTATE*)self;
    int got;
    if (frames > s->size) {
        if (s->buf != NULL) {
            SNDMEMI_free(s->buf);
        }
        s->size = frames;
        s->buf = (f32*)SNDMEMI_allocz(frames * 4);
    }
    got = s->f.next->process(s->f.next, frames, in, out, 1);
    if (got <= 0) {
        return got;
    }
    got = s->f.in2->process(s->f.in2, frames, in, s->buf, 2);
    if (got <= 0) {
        return got;
    }
    f32* dest = (f32*)out;
    for (int i = 0; i < frames; i++) {
        dest[i] += s->buf[i];
    }
    return frames;
}
