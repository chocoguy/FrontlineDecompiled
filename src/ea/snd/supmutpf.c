#include "ea/snd/smix.h"
#include "ea/snd/spktplay.h"
#include <string.h>

int SFILTER_unpackmtpf(void* self, int frames, void* in, void* out, int flag);

void SFILTER_unpackmtpfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKMTPFSTATE* s = (UNPACKMTPFSTATE*)state;
    s->f.process = SFILTER_unpackmtpf;
    s->player = SNDPKTPLAYI_voicetopackethandle(SNDDRV_getmastervoice(init->voice));
    s->chan = SNDDRV_getsamplechan(init->voice);
    s->data = NULL;
    s->pos = 0;
    s->len = 0;
    s->avail = 0;
    s->done = 0;
}

// Returns the frames produced, which are released on the next call.
int SFILTER_unpackmtpf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKMTPFSTATE* s = (UNPACKMTPFSTATE*)self;
    u8* dest = (u8*)out;
    int unused;

    if (s->done != 0) {
        SNDPKTPLAYI_freeframes(s->player, s->chan, s->done);
        s->done = 0;
    }
    while (frames > 0) {
        if (s->pos >= s->len) {
            s->data = (u8*)SNDPKTPLAYI_get(s->player, s->chan, &s->len, &unused);
            s->pos = 0;
            if (s->data == NULL) {
                if (s->done != 0) {
                    memset(dest, 0, frames * 4);
                }
                s->len = 0;
                return s->done;
            }
            if (s->data[0] != 0) {
                s->avail = 0;
                initmut((char*)s->data + 1, &s->ut);
            } else {
                // Continue the stream with the next packet's bits.
                s->ut.bits = s->data[1];
                s->ut.nbits = 8;
                s->ut.ptr = s->data + 2;
            }
        }
        if (s->avail == 0) {
            decodemut(&s->ut);
            s->avail = 432;
        }
        int n = s->len - s->pos;
        if (frames < n) {
            n = frames;
        }
        if (s->avail < n) {
            n = s->avail;
        }
        SNDI_memcpy(dest, &s->ut.frame[432 - s->avail], n * 4);
        dest += n * 4;
        frames -= n;
        s->pos += n;
        s->avail -= n;
        s->done += n;
    }
    return s->done;
}
