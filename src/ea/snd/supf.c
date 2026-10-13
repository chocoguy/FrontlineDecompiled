#include "ea/snd/smix.h"
#include "ea/snd/spktplay.h"
#include <string.h>

int SFILTER_unpackfgetframe(void* self) {
    return ((UNPACKFSTATE*)self)->pos;
}

// One-shot sample: silence after the end, then -1.
int SFILTER_unpackf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKFSTATE* s = (UNPACKFSTATE*)self;
    u32 pos = s->pos;
    if (pos >= s->end) {
        return -1;
    }
    s->pos = pos + frames;
    if (s->pos < s->end) {
        if (s->decode) {
            sndmix.decode(frames, s->data + pos, (f32*)out);
        }
    } else {
        int n = s->end - pos;
        if (s->decode) {
            sndmix.decode(n, s->data + pos, (f32*)out);
        }
        memset((f32*)out + n, 0, (frames - n) * 4);
    }
    return 1;
}

void SFILTER_unpackfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKFSTATE* s = (UNPACKFSTATE*)state;
    s->f.process = SFILTER_unpackf;
    s->data = (s16*)init->data;
    s->pos = 0;
    s->end = init->samples;
    s->decode = init->decode;
    init->getframe = (int (*)(SFILTER*))SFILTER_unpackfgetframe;
}
