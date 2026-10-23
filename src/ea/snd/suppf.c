#include "ea/snd/smix.h"
#include "ea/snd/spktplay.h"
#include <string.h>

// Packet-fed sample: returns the frames produced, which are released on the next call.
int SFILTER_unpackpf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKPFSTATE* s = (UNPACKPFSTATE*)self;
    f32* dest = (f32*)out;
    int samples;
    int unused;

    if (s->done != 0) {
        SNDPKTPLAYI_freeframes(s->player, s->chan, s->done);
        s->done = 0;
    }
    while (frames > 0) {
        if (s->pos >= s->len) {
            s->data = ADDR_TO_PTR(s16, SNDPKTPLAYI_get(s->player, s->chan, &samples, &unused));
            if (s->data == NULL) {
                if (s->done != 0) {
                    memset(dest, 0, frames * 4);
                }
                return s->done;
            }
            s->pos = 0;
            s->len = samples;
        }
        int n = s->len - s->pos;
        if (frames < n) {
            n = frames;
        }
        if (s->decode) {
            sndmix.decode(n, s->data + s->pos, dest);
        }
        dest += n;
        frames -= n;
        s->pos += n;
        s->done += n;
    }
    return s->done;
}

void SFILTER_unpackpfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKPFSTATE* s = (UNPACKPFSTATE*)state;
    s->f.process = SFILTER_unpackpf;
    s->player = SNDPKTPLAYI_voicetopackethandle(SNDDRV_getmastervoice(init->voice));
    s->chan = SNDDRV_getsamplechan(init->voice);
    s->data = NULL;
    s->len = -1;
    s->pos = 0;
    s->decode = init->decode;
    s->done = 0;
}
