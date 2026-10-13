#include "ea/snd/smix.h"

int SFILTER_unpacklfgetframe(void* self) {
    return ((UNPACKLFSTATE*)self)->pos;
}

// Looped sample: after loopEnd, continues from loopStart.
int SFILTER_unpacklf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKLFSTATE* s = (UNPACKLFSTATE*)self;
    f32* dest = (f32*)out;
    while (frames > 0) {
        u32 pos = s->pos;
        int n = s->loopEnd - pos + 1;
        if (frames < n) {
            n = frames;
        }
        if (s->decode) {
            sndmix.decode(n, s->data + pos, dest);
        }
        dest += n;
        frames -= n;
        s->pos += n;
        if (s->pos > s->loopEnd) {
            s->pos = s->loopStart;
        }
    }
    return 1;
}

void SFILTER_unpacklfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKLFSTATE* s = (UNPACKLFSTATE*)state;
    s->f.process = SFILTER_unpacklf;
    s->data = (s16*)init->data;
    s->pos = 0;
    s->loopStart = init->loopStart;
    s->loopEnd = init->loopEnd;
    s->decode = init->decode;
    init->getframe = (int (*)(SFILTER*))SFILTER_unpacklfgetframe;
}
