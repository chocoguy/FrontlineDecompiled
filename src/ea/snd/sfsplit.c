#include "ea/snd/smix.h"

void SFILTER_splitrestore(void* self);
int SFILTER_splitter(void* self, int frames, void* in, void* out, int flag);

int SFILTER_createSPLIT(SPLITSTATE* s) {
    s->f.next = NULL;
    s->f.in2 = NULL;
    s->f.prev = NULL;
    s->f.out2 = NULL;
    s->f.unk_0x1B = 0;
    s->f.destroy = (void (*)(SFILTER*))SFILTER_splitrestore;
    s->f.process = SFILTER_splitter;
    s->size = 0;
    s->buf = NULL;
    s->served = 0;
    s->fresh = 1;
    return 0;
}

void SFILTER_splitrestore(void* self) {
    SPLITSTATE* s = (SPLITSTATE*)self;
    if (s->buf != NULL) {
        SNDMEMI_free(s->buf);
    }
}

// The first pull of a slice reads upstream and keeps a copy; the second gets the copy.
int SFILTER_splitter(void* self, int frames, void* in, void* out, int flag) {
    SPLITSTATE* s = (SPLITSTATE*)self;
    if (s->served) {
        s->served = 0;
        s->fresh = 1;
    }
    int bytes = frames * 4;
    if (s->size < frames) {
        if (s->buf != NULL) {
            SNDMEMI_free(s->buf);
        }
        s->buf = (f32*)SNDMEMI_allocz(bytes);
        s->size = frames;
    }
    if (s->fresh) {
        int got = s->f.next->process(s->f.next, frames, out, in, 1);
        if (got <= 0) {
            return got;
        }
        SNDI_memcpy(out, in, bytes);
        SNDI_memcpy(s->buf, in, bytes);
        s->fresh = 0;
    } else {
        SNDI_memcpy(out, s->buf, bytes);
        s->served = 1;
    }
    return frames;
}
