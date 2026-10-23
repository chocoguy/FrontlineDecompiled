#include "ea/snd/smix.h"

// frac: cutoff as a fraction of the output rate; 1.0 or more removes the filter.
extern "C" void MIX_setlowpass(int voice, f32 frac) {
    int params[3];
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    if (frac < 1.0f) {
        if (mv->lpf == NULL) {
            mv->lpf = (LPFRCSTATE*)SNDMEMI_allocz(GC_SIZE(0x28, sizeof(LPFRCSTATE)));
            ((SFILTER*)mv->lpf)->destroy = NULL;
            ((SFILTER*)mv->lpf)->order = 0x28;
            SFILTER_createLPFRC(mv->lpf);
            SFILTER_add(&mv->chain, (SFILTER*)mv->lpf);
        }
        params[1] = sndgs.opts.caps.rate << 8;
        params[2] = 0x100;
        params[0] = (int)(frac * sndgs.opts.caps.rate) << 7;
        SFILTER_modifyLPFRC(mv->lpf, params);
    } else if (mv->lpf != NULL) {
        SFILTER_remove(&mv->chain, (SFILTER*)mv->lpf);
        SNDMEMI_free(mv->lpf);
        mv->lpf = NULL;
    }
}
