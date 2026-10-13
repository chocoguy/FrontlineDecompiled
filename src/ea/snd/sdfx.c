#include "ea/snd/snddrv.h"
#include "ea/snd/smix.h"

// The built-in reverb: four all-pass delays (100, 70, 49 and 34 ms).
u8 fxdefault[24] = {
    0x46, 0x58, 0x00, 0x00, 0x04, 0x00, 0x00, 0xFA, 0x00, 0x64, 0x40, 0x00,
    0x00, 0x46, 0x40, 0x5F, 0x00, 0x31, 0x4C, 0x00, 0x00, 0x22, 0x4C, 0x5F,
};

// Effects send: aux bus A for DSP voices, the CPU mixer's wet gain for mixer voices.
int SNDPLATFORM_setfxlevel(int voice, int bus) {
    CHANPUB* v = &sndgs.voices[voice];
    int i;
    if (v->renderMode & 0x200) {
        f32 level = 0.007874016f * ((s8)v->unk_0x45 * v->fxOut[bus]);
        for (i = 0; i < v->channels; i++) {
            SNDDRV_DSPMixerSetAuxBus(snddrv.voices[v->chanVoice[i]].axvpb, (int)level, 1);
        }
        return 0;
    }
    if (v->renderMode & 4) {
        f32 gain = 0.00000024030322f * ((s8)v->unk_0x45 * v->fxOut[bus]);
        for (i = 0; i < v->channels; i++) {
            SNDMIX_setwetgain(v->chanVoice[i] - sndgs.opts.caps.numHwVoices, bus, gain);
        }
        return 0;
    }
    return -15;
}

// Bus 0 type 0: no reverb; 1: the game's settings (param1); else the default.
static inline void setreverb() {
    SNDSYS_entercritical();
    if (sndgs.fxbus[0].type == 0) {
        sndmix.fxinit = NULL;
        sndmix.postmix = NULL;
        MIX_restorereverb();
    } else {
        MIXFXHEADER* hdr;
        if (sndgs.fxbus[0].type == 1) {
            hdr = (MIXFXHEADER*)sndgs.fxbus[0].param1;
        } else {
            hdr = (MIXFXHEADER*)fxdefault;
        }
        MIX_initreverb(sndgs.opts.caps.rate, hdr);
    }
    SNDSYS_leavecritical();
}

int SNDPLATFORM_fxinit(int bus, int type) {
    snddrv.fxcb = SNDDRV_setfx;
    if (!snddrv.mixerInited) {
        SNDDRV_initmixer();
    }
    if (sndgs.inited) {
        setreverb();
    }
    return 0;
}

void SNDDRV_setfx(int) {
    setreverb();
}
