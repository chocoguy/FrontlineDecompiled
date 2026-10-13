#include "ea/snd/smix.h"

int SFILTER_src(void* self, int frames, void* in, void* out, int flag);

int SFILTER_createSOURCE(SRCSTATE* src) {
    src->f.next = NULL;
    src->f.in2 = NULL;
    src->f.prev = NULL;
    src->f.out2 = NULL;
    src->f.unk_0x1B = 0;
    src->data = NULL;
    return 0;
}

void SFILTER_initSOURCE(SRCSTATE* src, void* data) {
    src->f.process = SFILTER_src;
    src->f.destroy = NULL;
    src->data = (u8*)data;
}

// Copies the next frames of 32-bit samples.
int SFILTER_src(void* self, int frames, void* in, void* out, int flag) {
    SRCSTATE* src = (SRCSTATE*)self;
    SNDI_memcpy(out, src->data, frames * 4);
    src->data += frames * 4;
    return frames;
}
