#include "ea/snd/sndi.h"
#include <string.h>

// The defaults come from the platform's output caps the first time.
extern "C" int SNDSYS_getopts(SNDSYSOPTS* opts) {
    static int gotcaps = 0;
    static int ret = 0;

    if (gotcaps == 0) {
        ret = SNDPLATFORM_outputcaps();
        gotcaps = 1;
        sndgs.opts.caps.maxBanks = 16;
        sndgs.opts.caps.unk_0x2C = 90;
        sndgs.opts.caps.stealEqual = 1;
        sndgs.caps = sndgs.opts.caps;
    }
    *opts = sndgs.opts;
    return ret;
}

extern "C" int SNDSYS_setopts(SNDSYSOPTS* opts) {
    sndgs.opts.caps = opts->caps;
    sndgs.opts.aramalloc = opts->aramalloc;
    sndgs.opts.aramfree = opts->aramfree;
    sndgs.opts.unk_0xC0 = opts->unk_0xC0;
    sndgs.opts.abortmsg = opts->abortmsg;
    sndgs.opts.unk_0xC8[0] = opts->unk_0xC8[0];
    sndgs.opts.unk_0xC8[1] = opts->unk_0xC8[1];
    SNDPLATFORM_outputset();
    sndgs.caps = sndgs.opts.caps;
    return 0;
}

extern "C" int SNDSYSI_init(void* mem, int size, int) {
    int result;

    sndlibauthor[0] = 'S';
    if (sndgs.inited) {
        return 0;
    }
    memset(mem, 0, size);
    SNDMEMI_init(mem, size);
    if (sndgs.numVoices == 0) {
        result = SNDSYS_getopts(&sndgs.opts);
        if (result < 0) {
            return result;
        }
        SNDSYS_setopts(&sndgs.opts);
    }
    SNDI_randomseed(sndgs.opts.caps.seed);
    SNDI_mutexalloc();
    SNDSYS_entercritical();
    sndgs.voices = (SNDVOICE*)SNDMEMI_allocz(sndgs.numVoices << 7);
    sndgs.banks = (SNDBANKSLOT*)SNDMEMI_allocz(sndgs.opts.caps.maxBanks * GC_SIZE(0xC, sizeof(SNDBANKSLOT)));
    SNDSYS_leavecritical();
    sndgs.serverTicks = 0;
    sndgs.masterVol = 0x7F;
    sndgs.numUserDataClients = 0;
    sndgs.num100hzClients = 0;
    sndgs.numServerClients = 0;

    result = SNDPLATFORM_init();
    if (result < 0) {
        SNDPLATFORM_restore();
        SNDI_mutexfree();
        return result;
    }
    sndgs.inited = 1;
    if (sndgs.opts.caps.speakers == 2) {
        sndgs.chanAzimuth[1][1] = 0x4000;
        sndgs.chanAzimuth[1][0] = (s16)0xC000;
    } else {
        sndgs.chanAzimuth[1][1] = 0x2000;
        sndgs.chanAzimuth[1][0] = (s16)0xE000;
    }
    sndgs.chanAzimuth[2][0] = sndgs.opts.caps.speakerAz[2][2];
    sndgs.chanAzimuth[2][1] = sndgs.opts.caps.speakerAz[2][0];
    sndgs.chanAzimuth[2][2] = sndgs.opts.caps.speakerAz[2][1];
    sndgs.chanAzimuth[3][0] = 0xE000;
    sndgs.chanAzimuth[3][1] = 0x2000;
    sndgs.chanAzimuth[3][2] = 0xA000;
    sndgs.chanAzimuth[3][3] = 0x6000;
    SNDI_precalcaztospkrvol();
    return 0;
}

extern "C" int SNDSYS_restore(void) {
    int result;

    if (SNDSYS_inited() == 0) {
        return -14;
    }
    SNDfxinitbus(0, 0, 0, -1, -1);
    if (sndgs.restoreHook0 != NULL) {
        sndgs.restoreHook0();
    }
    if (sndgs.restoreHook1 != NULL) {
        sndgs.restoreHook1();
    }
    if (sndgs.restoreHook2 != NULL) {
        sndgs.restoreHook2();
    }
    if (sndgs.restoreHook5 != NULL) {
        sndgs.restoreHook5();
    }
    if (sndgs.restoreHook6 != NULL) {
        sndgs.restoreHook6();
    }
    if (sndgs.restoreHook4 != NULL) {
        sndgs.restoreHook4();
    }
    SNDstopall();
    if (sndgs.restoreHook3 != NULL) {
        sndgs.restoreHook3(-1);
    }
    SNDPLATFORM_restore();
    SNDSYS_entercritical();
    SNDMEMI_free(sndgs.voices);
    SNDMEMI_free(sndgs.banks);
    SNDSYS_leavecritical();
    result = SNDMEMI_restore();
    sndgs.inited = 0;
    SNDI_mutexfree();
    return result;
}

extern "C" int SNDSYS_inited(void) {
    return sndgs.inited;
}
