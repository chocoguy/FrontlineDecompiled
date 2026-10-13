#include "ea/sys/ipad.h"
#include "ea/real.h"
#include "dolphin/os.h"
#include "dolphin/si.h"

// Status written by the sampling callback (interrupt time) and the copy the game reads.
PADStatus vgPadStatus[4];
PADStatus gPadStatus[4];
int giPADStarted;
PADSamplingCallback OldCallBack;

// Sampling callback: keep the last good status of each pad.
void iPADVBLUpdate() {
    PADStatus status[4];
    PADStatus* src;
    PADStatus* dst;
    int i;

    PADRead(status);
    PADClamp(status);
    src = status;
    dst = vgPadStatus;
    for (i = 0; i < 4; i++) {
        if (src->err != PAD_ERR_TRANSFER) {
            MEM_copy(dst, src, sizeof(PADStatus));
        }
        dst++;
        src++;
    }
}

int iPAD_init() {
    if (giPADStarted == 0) {
        giPADStarted = 1;
        MEM_fill(vgPadStatus, 0, sizeof(vgPadStatus));
        MEM_fill(gPadStatus, 0, sizeof(gPadStatus));
        vgPadStatus[0].err = PAD_ERR_NO_CONTROLLER;
        vgPadStatus[1].err = PAD_ERR_NO_CONTROLLER;
        vgPadStatus[2].err = PAD_ERR_NO_CONTROLLER;
        gPadStatus[0].err = PAD_ERR_NO_CONTROLLER;
        gPadStatus[1].err = PAD_ERR_NO_CONTROLLER;
        gPadStatus[2].err = PAD_ERR_NO_CONTROLLER;
        vgPadStatus[3].err = PAD_ERR_NO_CONTROLLER;
        gPadStatus[3].err = PAD_ERR_NO_CONTROLLER;
        OldCallBack = PADSetSamplingCallback(iPADVBLUpdate);
        SISetSamplingRate(0);
    }
    return 1;
}

// Latch the sampled status; reset pads that were plugged in since the last frame.
void iPAD_update() {
    BOOL level;
    PADStatus* cur;
    PADStatus* next;
    u32 resetMask;
    int i;

    level = OSDisableInterrupts();
    MEM_copy(gPadStatus, vgPadStatus, sizeof(gPadStatus));
    OSRestoreInterrupts(level);

    cur = gPadStatus;
    resetMask = 0;
    next = vgPadStatus;
    for (i = 0; i < 4; i++) {
        u32 bit = PAD_CHAN0_BIT >> i;
        switch (SIProbe(i)) {
        case SI_GC_CONTROLLER:
        case SI_GC_WAVEBIRD:
            if (cur->err == PAD_ERR_NO_CONTROLLER) {
                next->err = PAD_ERR_NOT_READY;
                resetMask |= bit;
            }
            break;
        default:
            next->err = PAD_ERR_NO_CONTROLLER;
            break;
        }
        next++;
        cur++;
    }
    if (resetMask != 0) {
        PADReset(resetMask);
    }
}
