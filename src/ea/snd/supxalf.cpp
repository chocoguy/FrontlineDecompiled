#include "ea/snd/smix.h"

int SFILTER_unpackxalf(void* self, int frames, void* in, void* out, int flag);
void SFILTER_unpackxalfrestore(void* self);
int SFILTER_unpackgetframexalf(void* self);

void SFILTER_unpackxalfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKXALFSTATE* s = (UNPACKXALFSTATE*)state;
    s->f.process = SFILTER_unpackxalf;
    s->f.destroy = (void (*)(SFILTER*))SFILTER_unpackxalfrestore;
    s->data = (u8*)init->data;
    s->samples = init->samples;
    s->pos = 0;
    s->looped = 0;
    s->started = 0;
    s->loopStart = init->loopStart;
    s->loopEnd = init->loopEnd;
    s->dec = new SND::CEAXABLKDecf;
    init->getframe = (int (*)(SFILTER*))SFILTER_unpackgetframexalf;
}

void SFILTER_unpackxalfrestore(void* self) {
    UNPACKXALFSTATE* s = (UNPACKXALFSTATE*)self;
    if (s->dec != NULL) {
        delete s->dec;
    }
}

int SFILTER_unpackgetframexalf(void* self) {
    return ((UNPACKXALFSTATE*)self)->pos;
}

// Looped sample: plays up to the loop start's block, then loops its blocks to loopEnd. The
// decoder history at the loop block is saved on the first pass and restored on later ones.
int SFILTER_unpackxalf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKXALFSTATE* s = (UNPACKXALFSTATE*)self;
    f32* dest = (f32*)out;
    int done = 0;

    if (!s->started) {
        int n = s->loopStart / 28 * 28;
        s->dec->Feed(s->data, n * 4, n);
        s->started = 1;
    }
    while (frames != 0) {
        int got = s->dec->Decode(&dest, frames);
        done += got;
        dest += got;
        s->pos += got;
        if (got < frames) {
            frames -= got;
            int skip = s->loopStart % 28;
            s->pos = s->loopStart - skip;
            int n = s->loopEnd - s->pos + 1;
            s->dec->Feed(s->data + s->pos / 28 * 15, n * 4, n);
            if (!s->looped) {
                s->loopState = s->dec->GetState();
                got = s->dec->Decode(&dest, frames);
                done += got;
                frames -= got;
                dest += got;
                s->pos += got;
                s->looped = 1;
            } else {
                f32 scratch[28];
                f32* p;
                SND::XAFSTATE state = s->loopState;
                s->dec->SetState(&state);
                p = scratch;
                s->pos += s->dec->Decode(&p, skip);
            }
        } else {
            frames -= got;
        }
    }
    return done;
}
