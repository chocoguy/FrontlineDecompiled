#include "ea/snd/smix.h"

// freq <= 0 removes the filter.
extern "C" void MIX_sethighpass(int voice, int freq) {
    int params[2];
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    if (freq > 0) {
        if (mv->hpf == NULL) {
            mv->hpf = (HPFSTATE*)SNDMEMI_allocz(0x58);
            ((SFILTER*)mv->hpf)->destroy = NULL;
            ((SFILTER*)mv->hpf)->order = 0x50;
            SFILTER_createHPFFIR8(mv->hpf);
            SFILTER_add(&mv->chain, (SFILTER*)mv->hpf);
        }
        params[0] = freq << 8;
        params[1] = sndgs.opts.caps.rate << 8;
        SFILTER_modifyHPFFIR8(mv->hpf, params);
    } else if (mv->hpf != NULL) {
        SFILTER_remove(&mv->chain, (SFILTER*)mv->hpf);
        SNDMEMI_free(mv->hpf);
        mv->hpf = NULL;
    }
}
