#include "ea/snd/smix.h"
#include "ea/snd/spktplay.h"
#include <string.h>

int SFILTER_unpackxapf(void* self, int frames, void* in, void* out, int flag);
void SFILTER_unpackxapfrestore(void* self);

void SFILTER_unpackxapfinit(void* state, UNPACKINITPARAMS* init) {
    UNPACKXAPFSTATE* s = (UNPACKXAPFSTATE*)state;
    s->f.process = SFILTER_unpackxapf;
    s->f.destroy = (void (*)(SFILTER*))SFILTER_unpackxapfrestore;
    s->player = SNDPKTPLAYI_voicetopackethandle(SNDDRV_getmastervoice(init->voice));
    s->chan = SNDDRV_getsamplechan(init->voice);
    s->data = NULL;
    s->samples = init->samples;
    s->pos = 0;
    s->done = 0;
    s->dec = new SND::CEAXABLKDecf;
}

void SFILTER_unpackxapfrestore(void* self) {
    UNPACKXAPFSTATE* s = (UNPACKXAPFSTATE*)self;
    if (s->dec != NULL) {
        delete s->dec;
    }
}

// Packet-fed sample: returns the frames produced, which are released on the next call.
int SFILTER_unpackxapf(void* self, int frames, void* in, void* out, int flag) {
    UNPACKXAPFSTATE* s = (UNPACKXAPFSTATE*)self;
    f32* dest = (f32*)out;
    int done = 0;
    int unused;

    if (s->done != 0) {
        SNDPKTPLAYI_freeframes(s->player, s->chan, s->done);
        s->done = 0;
    }
    while (frames > 0) {
        int got = s->dec->Decode(&dest, frames);
        done += got;
        s->pos += got;
        dest += got;
        s->done += got;
        if (got < frames) {
            frames -= got;
            s->data = ADDR_TO_PTR(u8, SNDPKTPLAYI_get(s->player, s->chan, &s->samples, &unused));
            if (s->data != NULL) {
                SND::XAFSTATE state;
                state.s1 = ((s16*)s->data)[0];
                state.s2 = ((s16*)s->data)[1];
                s->dec->SetState(&state);
                s->data += 4;
                s->pos = 0;
                s->dec->Feed(s->data, s->samples * 4, s->samples);
            } else {
                if (frames > 0 && s->done != 0) {
                    memset(dest, 0, frames * 4);
                    done += frames;
                }
                frames = 0;
            }
        } else {
            frames -= got;
        }
    }
    return done;
}
