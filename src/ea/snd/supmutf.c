#include "ea/snd/smix.h"

int SFILTER_unpackmtf(void* self, int frames, void* in, void* out, int flag);
int SFILTER_unpackgetframemtf(void* self);

void SFILTER_unpackmtfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKMTFSTATE* s = (UNPACKMTFSTATE*)state;
    s->f.process = SFILTER_unpackmtf;
    s->samples = init->samples;
    s->pos = 0;
    init->getframe = (int (*)(SFILTER*))SFILTER_unpackgetframemtf;
    initmut((char*)init->data, &s->ut);
    s->avail = 0;
}

// One-shot stream: silence after the end, then -1.
int SFILTER_unpackmtf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKMTFSTATE* s = (UNPACKMTFSTATE*)self;
    f32* dest = (f32*)out;
    u32 pos = s->pos;
    if (pos >= s->samples) {
        return -1;
    }
    s->pos = pos + frames;
    int over = s->pos - s->samples;
    if (over <= 0) {
        over = 0;
    } else {
        frames -= over;
    }
    int n = s->avail;
    while (frames > 0) {
        if (frames < n) {
            n = frames;
        }
        SNDI_memcpy(dest, &s->ut.frame[432 - s->avail], n * 4);
        frames -= n;
        dest += n;
        s->avail -= n;
        if (frames > 0) {
            decodemut(&s->ut);
            n = 432;
            s->avail = 432;
        }
    }
    for (int i = 0; i < over; i++) {
        *dest++ = 0.0f;
    }
    return 1;
}

int SFILTER_unpackgetframemtf(void* self) {
    return ((UNPACKMTFSTATE*)self)->pos;
}
