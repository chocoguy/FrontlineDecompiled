#include "ea/snd/smix.h"
#include <string.h>

int SFILTER_unpackxaf(void* self, int frames, void* in, void* out, int flag);
void SFILTER_unpackxafrestore(void* self);
int SFILTER_unpackgetframexaf(void* self);

void SFILTER_unpackxafinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKXAFSTATE* s = (UNPACKXAFSTATE*)state;
    s->f.process = SFILTER_unpackxaf;
    s->f.destroy = (void (*)(SFILTER*))SFILTER_unpackxafrestore;
    s->data = (u8*)init->data;
    s->samples = init->samples;
    s->pos = 0;
    s->dec = new SND::CEAXABLKDecf;
    init->getframe = (int (*)(SFILTER*))SFILTER_unpackgetframexaf;
    s->dec->Feed(s->data, s->samples * 4, s->samples);
}

void SFILTER_unpackxafrestore(void* self) {
    UNPACKXAFSTATE* s = (UNPACKXAFSTATE*)self;
    if (s->dec != NULL) {
        delete s->dec;
    }
}

int SFILTER_unpackgetframexaf(void* self) {
    return ((UNPACKXAFSTATE*)self)->pos;
}

// One-shot sample: silence after the end, then -1.
int SFILTER_unpackxaf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKXAFSTATE* s = (UNPACKXAFSTATE*)self;
    f32* dest = (f32*)out;
    if (s->pos >= s->samples) {
        return -1;
    }
    int got = s->dec->Decode(&dest, frames);
    int result = got;
    s->pos += got;
    dest += got;
    if (got < frames) {
        int n = frames - got;
        memset(dest, 0, n * 4);
        result += n;
    }
    return result;
}
