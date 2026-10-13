#include "ea/snd/sndi.h"
#include <dolphin/ax.h>
#include <dolphin/os.h>

// Per-voice DSP mix state (after the SDK's MIX library): each gain has a current value and
// a target; changes ramp over one 320-sample frame.
#define MIX_MUTE 0x4
#define MIX_MIX_PENDING 0x80000000  // copy targets to current
#define MIX_MIX_CHANGED 0x40000000
#define MIX_VOL_PENDING 0x20000000
#define MIX_VOL_CHANGED 0x10000000

struct MIXCHANNEL {
    /* 0x00 */ AXVPB* axvpb;
    /* 0x04 */ u32 mode;
    /* 0x08 */ s32 input;
    /* 0x0C */ s32 unk_0xC;
    /* 0x10 */ s32 unk_0x10;
    /* 0x14 */ u8 unk_0x14[0x14];
    /* 0x28 */ u16 v, v1;
    /* 0x2C */ u16 vL, vL1;
    /* 0x30 */ u16 vR, vR1;
    /* 0x34 */ u16 vS, vS1;
    /* 0x38 */ u16 vAL, vAL1;
    /* 0x3C */ u16 vAR, vAR1;
    /* 0x40 */ u16 vAS, vAS1;
    /* 0x44 */ u16 vBL, vBL1;
    /* 0x48 */ u16 vBR, vBR1;
    /* 0x4C */ u16 vBS, vBS1;
};

static MIXCHANNEL __MIXChannel[AX_MAX_VOICES];
static s32 numoutputchannels;

void SNDDRV_DSPMixerUpdate() {
    MIXCHANNEL* c = __MIXChannel;
    for (int i = 0; i < AX_MAX_VOICES; i++, c++) {
        AXVPB* p = c->axvpb;
        int setVol = 0;
        int setMix = 0;
        if (p == NULL) {
            continue;
        }
        if (c->mode & MIX_VOL_PENDING) {
            setVol = 1;
            c->v = c->v1;
            c->mode &= ~MIX_VOL_PENDING;
        }
        if (c->mode & MIX_VOL_CHANGED) {
            if (c->mode & MIX_MUTE) {
                c->v1 = 0;
            } else {
                c->v1 = c->input;
            }
            setVol = 1;
            c->mode |= MIX_VOL_PENDING;
        }
        if (c->mode & MIX_MIX_PENDING) {
            setMix = 1;
            c->vL = c->vL1;
            c->vR = c->vR1;
            c->vS = c->vS1;
            c->vAL = c->vAL1;
            c->vAR = c->vAR1;
            c->vAS = c->vAS1;
            c->vBL = c->vBL1;
            c->vBR = c->vBR1;
            c->vBS = c->vBS1;
            c->mode &= ~MIX_MIX_PENDING;
        }
        if (c->mode & MIX_MIX_CHANGED) {
            setMix = 1;
            c->mode &= ~MIX_MIX_CHANGED;
            c->mode |= MIX_MIX_PENDING;
        }
        if (setVol && p != NULL) {
            p->pb.ve.currentVolume = c->v;
            p->pb.ve.currentDelta = (c->v1 - c->v) / 320;
            p->sync |= AX_SYNC_FLAG_COPYVOL;
        }
        if (setMix && p != NULL) {
            AXPBMIX* m = &p->pb.mix;
            m->vL = c->vL;
            m->vDeltaL = (c->vL1 - c->vL) / 320;
            m->vR = c->vR;
            m->vDeltaR = (c->vR1 - c->vR) / 320;
            m->vAuxAL = c->vAL;
            m->vDeltaAuxAL = (c->vAL1 - c->vAL) / 320;
            m->vAuxAR = c->vAR;
            m->vDeltaAuxAR = (c->vAR1 - c->vAR) / 320;
            m->vAuxBL = c->vBL;
            m->vDeltaAuxBL = (c->vBL1 - c->vBL) / 320;
            m->vAuxBR = c->vBR;
            m->vDeltaAuxBR = (c->vBR1 - c->vBR) / 320;
            m->vAuxBS = c->vBS;
            m->vDeltaAuxBS = (c->vBS1 - c->vBS) / 320;
            m->vS = c->vS;
            m->vDeltaS = (c->vS1 - c->vS) / 320;
            u16 ctrl = 0;
            m->vAuxAS = c->vAS;
            m->vDeltaAuxAS = (c->vAS1 - c->vAS) / 320;
            if (m->vAuxAL != 0 || m->vAuxAR != 0 || m->vAuxAS != 0) {
                ctrl = 1;
            }
            if (m->vAuxBL != 0 || m->vAuxBR != 0 || m->vAuxBS != 0) {
                ctrl |= 2;
            }
            if (m->vS != 0 || m->vAuxAS != 0 || m->vAuxBS != 0) {
                ctrl |= 4;
            }
            if (m->vDeltaL != 0 || m->vDeltaR != 0 || m->vDeltaS != 0 || m->vDeltaAuxAL != 0 ||
                m->vDeltaAuxAR != 0 || m->vDeltaAuxAS != 0 || m->vDeltaAuxBL != 0 || m->vDeltaAuxBR != 0 ||
                m->vDeltaAuxBS != 0) {
                ctrl |= 8;
            }
            p->pb.mixerCtrl = ctrl;
            p->sync |= AX_SYNC_FLAG_COPYMXRCTRL | AX_SYNC_FLAG_COPYMIX;
        }
    }
}

void SNDDRV_DSPMixerSetVol(AXVPB* p, int vol) {
    MIXCHANNEL* c = &__MIXChannel[p->index];
    c->input = vol;
    c->mode |= MIX_VOL_CHANGED;
}

void SNDDRV_DSPMixerReleaseChannel(AXVPB* p) {
    __MIXChannel[p->index].axvpb = NULL;
}

void SNDDRV_DSPMixerInitChannel(AXVPB* p, unsigned long mode, int input, unsigned short auxA,
                                unsigned short auxB) {
    MIXCHANNEL* c = &__MIXChannel[p->index];
    c->axvpb = p;
    c->mode = mode & MIX_MUTE;
    c->input = input;
    c->vAL1 = auxA;
    c->vAR1 = auxA;
    c->vBL1 = auxB;
    c->vBR1 = auxB;
    if (numoutputchannels > 2) {
        c->vAS1 = auxA;
        c->vBS1 = auxB;
    }
    if (c->mode & MIX_MUTE) {
        c->v = 0;
    } else {
        c->v = input;
    }
    BOOL old = OSDisableInterrupts();
    u16 ctrl = 0;
    p->pb.ve.currentVolume = c->v;
    p->pb.ve.currentDelta = 0;
    p->pb.mix.vL = c->vL;
    p->pb.mix.vDeltaL = 0;
    p->pb.mix.vR = c->vR;
    p->pb.mix.vDeltaR = 0;
    p->pb.mix.vAuxAL = c->vS;
    p->pb.mix.vDeltaAuxAL = 0;
    if (p->pb.mix.vS != 0 || p->pb.mix.vAuxAS != 0 || p->pb.mix.vAuxBS != 0) {
        ctrl = 4;
    }
    p->pb.mixerCtrl = ctrl;
    p->sync |= AX_SYNC_FLAG_COPYVOL | AX_SYNC_FLAG_COPYMIX | AX_SYNC_FLAG_COPYMXRCTRL;
    OSRestoreInterrupts(old);
}

void SNDDRV_DSPMixerInit(int outputs) {
    MIXCHANNEL* c = __MIXChannel;
    for (int i = 0; i < AX_MAX_VOICES; i++, c++) {
        c->mode = MIX_MIX_CHANGED | MIX_VOL_CHANGED;
        c->input = 0;
        c->unk_0xC = 0;
        c->unk_0x10 = 0;
        c->vBS = 0;
        c->vBR = 0;
        c->vBL = 0;
        c->vAS = 0;
        c->vAR = 0;
        c->vAL = 0;
        c->vS = 0;
        c->vR = 0;
        c->vL = 0;
        c->v = 0;
    }
    numoutputchannels = outputs;
}

// bus 1: aux A, 2: aux B.
void SNDDRV_DSPMixerSetAuxBus(AXVPB* p, unsigned short level, int bus) {
    MIXCHANNEL* c = &__MIXChannel[p->index];
    if (bus == 1) {
        c->vAR1 = level;
        c->vAL1 = level;
        if (numoutputchannels == 3) {
            c->vAS1 = level;
        }
    } else if (bus == 2) {
        c->vBR1 = level;
        c->vBL1 = level;
        if (numoutputchannels == 3) {
            c->vBS1 = level;
        }
    }
    c->mode |= MIX_MIX_CHANGED;
}

// vols: per speaker, as SNDI_aztospkrvol.
void SNDDRV_DSPMixerSetPan(AXVPB* p, unsigned short* vols) {
    MIXCHANNEL* c = &__MIXChannel[p->index];
    c->vR1 = vols[0];
    if (numoutputchannels != 3) {
        c->vL1 = vols[1];
    } else {
        c->vS1 = vols[1];
        c->vL1 = vols[2];
    }
    c->mode |= MIX_MIX_CHANGED;
}
