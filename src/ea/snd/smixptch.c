#include "ea/snd/smix.h"

// Adds a resampler to the voice's chain the first time. pitch: 4.16, at most 4.0.
extern "C" void MIX_setpitch(int voice, int pitch) {
    int hq = 0;
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    if (pitch > 0x40000) {
        pitch = 0x40000;
    }
    if (mv->rsf == NULL) {
        mv->rsf = (RSFSTATE*)SNDMEMI_allocz(0x3C);
        if (sndmix.quality >= 50) {
            hq = 1;
        }
        ((SFILTER*)mv->rsf)->destroy = NULL;
        ((SFILTER*)mv->rsf)->order = 0xA0;
        SFILTER_rsfinit(mv->rsf, sndgs.unk_0x16F, hq);
        SFILTER_add(&mv->chain, (SFILTER*)mv->rsf);
    }
    SFILTER_rsfsetpitch(mv->rsf, pitch);
}
