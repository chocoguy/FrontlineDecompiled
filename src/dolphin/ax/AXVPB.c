#include <dolphin/ax.h>
#include <dolphin/os.h>

static AXPB __AXPB[AX_MAX_VOICES] __attribute__((aligned(32)));
static u32 __AXITD[AX_MAX_VOICES][16] __attribute__((aligned(32)));
static u16 __AXUpdates[AX_MAX_VOICES][128] __attribute__((aligned(32)));
static AXVPB __AXVPB[AX_MAX_VOICES];

static u32 __AXMaxDspCycles;
static u32 __AXRecDspCycles;
static u32 __AXNumVoices;

// DSP cycles per voice, by sample rate converter ratio and by mixer control.
static u32 __AXSrcCycles[5] = {0xDF8, 0xF78, 0x14B8, 0x19F8, 0x19F8};
static u32 __AXMixCycles[32] = {
    0x5BE,  0xB7C,  0xB7C,  0x113A, 0x8B6,  0x116C, 0x116C, 0x1A22, 0x9A6,  0x134C, 0x134C,
    0x1CF2, 0xE97,  0x1D2E, 0x1D2E, 0x2BC5, 0xB7C,  0x1432, 0xB7C,  0x1432, 0xB7C,  0xB7C,
    0xB7C,  0xB7C,  0x134C, 0x21E3, 0x134C, 0x21E3, 0x134C, 0x21E3, 0x134C, 0x21E3,
};

u32 __AXGetNumVoices(void) {
    return __AXNumVoices;
}

// Copies the fields the sync flags mark from the voice to the DSP's parameter block.
void __AXServiceVPB(AXVPB* pvpb) {
    AXPB* dst;
    AXPB* src;
    u32 sync;
    u32 i;

    __AXNumVoices++;
    src = &pvpb->pb;
    sync = pvpb->sync;
    dst = &__AXPB[pvpb->index];
    if (sync == 0) {
        src->state = dst->state;
        src->ve.currentVolume = dst->ve.currentVolume;
        src->addr.currentAddressHi = dst->addr.currentAddressHi;
        src->addr.currentAddressLo = dst->addr.currentAddressLo;
        return;
    }
    if (sync & AX_SYNC_FLAG_COPYALL) {
        u32* s = (u32*)src;
        u32* d = (u32*)dst;
        for (i = 0; i < sizeof(AXPB) / 4; i++) {
            d[i] = s[i];
        }
        if (pvpb->updateCounter != 0) {
            u32* us = (u32*)pvpb->updateData;
            u32* ud = (u32*)__AXUpdates[pvpb->index];
            for (i = 0; i < pvpb->updateCounter; i++) {
                *us++ = *ud++;
            }
        }
        return;
    }
    if (sync & AX_SYNC_FLAG_COPYSELECT) {
        dst->srcSelect = src->srcSelect;
        dst->coefSelect = src->coefSelect;
    }
    if (sync & AX_SYNC_FLAG_COPYMXRCTRL) {
        dst->mixerCtrl = src->mixerCtrl;
    }
    if (sync & AX_SYNC_FLAG_COPYSTATE) {
        dst->state = src->state;
    } else {
        src->state = dst->state;
    }
    if (sync & AX_SYNC_FLAG_COPYTYPE) {
        dst->type = src->type;
    }
    if (sync & AX_SYNC_FLAG_COPYMIX) {
        u16* s = (u16*)&src->mix;
        u16* d = (u16*)&dst->mix;
        for (i = 0; i < sizeof(AXPBMIX) / 2; i++) {
            d[i] = s[i];
        }
    }
    if (sync & AX_SYNC_FLAG_COPYTSHIFT) {
        dst->itd.targetShiftL = src->itd.targetShiftL;
        dst->itd.targetShiftR = src->itd.targetShiftR;
    } else if (sync & AX_SYNC_FLAG_COPYITD) {
        u32* itd;
        dst->itd.flag = src->itd.flag;
        dst->itd.bufferHi = src->itd.bufferHi;
        dst->itd.bufferLo = src->itd.bufferLo;
        dst->itd.shiftL = src->itd.shiftL;
        dst->itd.shiftR = src->itd.shiftR;
        dst->itd.targetShiftL = src->itd.targetShiftL;
        dst->itd.targetShiftR = src->itd.targetShiftR;
        itd = (u32*)pvpb->itdBuffer;
        for (i = 0; i < 16; i++) {
            itd[i] = 0;
        }
    }
    if (sync & AX_SYNC_FLAG_COPYUPDATE) {
        dst->update.updNum[0] = src->update.updNum[0];
        dst->update.updNum[1] = src->update.updNum[1];
        dst->update.updNum[2] = src->update.updNum[2];
        dst->update.updNum[3] = src->update.updNum[3];
        dst->update.updNum[4] = src->update.updNum[4];
        if (pvpb->updateCounter != 0) {
            u32* us = (u32*)pvpb->updateData;
            u32* ud = (u32*)__AXUpdates[pvpb->index];
            for (i = 0; i < pvpb->updateCounter; i++) {
                *ud++ = *us++;
            }
        }
    }
    if (sync & AX_SYNC_FLAG_COPYDPOP) {
        dst->dpop = src->dpop;
    }
    if (sync & AX_SYNC_FLAG_COPYDELTA) {
        src->ve.currentVolume = dst->ve.currentVolume;
        dst->ve.currentDelta = src->ve.currentDelta;
    } else if (sync & AX_SYNC_FLAG_COPYVOL) {
        dst->ve.currentVolume = src->ve.currentVolume;
        dst->ve.currentDelta = src->ve.currentDelta;
    }
    if (sync & AX_SYNC_FLAG_COPYFIR) {
        dst->fir = src->fir;
    }
    if (sync & (AX_SYNC_FLAG_COPYLOOP | AX_SYNC_FLAG_COPYLOOPADDR | AX_SYNC_FLAG_COPYENDADDR |
                AX_SYNC_FLAG_COPYCURADDR)) {
        if (sync & AX_SYNC_FLAG_COPYLOOP) {
            dst->addr.loopFlag = src->addr.loopFlag;
        }
        if (sync & AX_SYNC_FLAG_COPYLOOPADDR) {
            *(u32*)&dst->addr.loopAddressHi = *(u32*)&src->addr.loopAddressHi;
        }
        if (sync & AX_SYNC_FLAG_COPYENDADDR) {
            *(u32*)&dst->addr.endAddressHi = *(u32*)&src->addr.endAddressHi;
        }
        if (sync & AX_SYNC_FLAG_COPYCURADDR) {
            *(u32*)&dst->addr.currentAddressHi = *(u32*)&src->addr.currentAddressHi;
        }
    } else if (sync & AX_SYNC_FLAG_COPYADDR) {
        *(u32*)&dst->addr.loopFlag = *(u32*)&src->addr.loopFlag;
        *(u32*)&dst->addr.loopAddressHi = *(u32*)&src->addr.loopAddressHi;
        *(u32*)&dst->addr.endAddressHi = *(u32*)&src->addr.endAddressHi;
        *(u32*)&dst->addr.currentAddressHi = *(u32*)&src->addr.currentAddressHi;
    } else {
        src->addr.currentAddressHi = dst->addr.currentAddressHi;
        src->addr.currentAddressLo = dst->addr.currentAddressLo;
    }
    if (sync & AX_SYNC_FLAG_COPYADPCM) {
        u32* s = (u32*)&src->adpcm;
        u32* d = (u32*)&dst->adpcm;
        for (i = 0; i < sizeof(AXPBADPCM) / 4; i++) {
            d[i] = s[i];
        }
    }
    if (sync & AX_SYNC_FLAG_COPYRATIO) {
        dst->src.ratioHi = src->src.ratioHi;
        dst->src.ratioLo = src->src.ratioLo;
    } else if (sync & AX_SYNC_FLAG_COPYSRC) {
        dst->src = src->src;
    }
    if (sync & AX_SYNC_FLAG_COPYADPCMLOOP) {
        dst->adpcmLoop = src->adpcmLoop;
    }
}

// Services the voices from the highest priority down until the frame's DSP budget is used;
// the rest are stopped and their callbacks queued.
void __AXSyncPBs(u32 lessDspCycles) {
    u32 cycles;
    u32 i;
    AXVPB* pvpb;

    __AXNumVoices = 0;
    DCInvalidateRange(__AXPB, sizeof(__AXPB));
    DCInvalidateRange(__AXITD, sizeof(__AXITD));
    cycles = __AXGetCommandListCycles() + 0x10000 + lessDspCycles - 0x55F0;
    for (i = AX_PRIORITY_STACKS - 1; i != 0; i--) {
        for (pvpb = __AXGetStackHead(i); pvpb != NULL; pvpb = (AXVPB*)pvpb->next) {
            if (pvpb->depop) {
                __AXDepopVoice(&__AXPB[pvpb->index]);
            }
            if (pvpb->pb.state == AX_PB_STATE_RUN || pvpb->updateCounter != 0) {
                cycles += __AXSrcCycles[pvpb->pb.src.ratioHi] + __AXMixCycles[pvpb->pb.mixerCtrl] + 0x8C;
                if (__AXMaxDspCycles > cycles) {
                    __AXServiceVPB(pvpb);
                } else {
                    AXPB* ppb = &__AXPB[pvpb->index];
                    if (ppb->state == AX_PB_STATE_RUN) {
                        __AXDepopVoice(ppb);
                    }
                    ppb->update.updNum[4] = 0;
                    ppb->update.updNum[3] = 0;
                    ppb->update.updNum[2] = 0;
                    ppb->update.updNum[1] = 0;
                    ppb->update.updNum[0] = 0;
                    ppb->state = AX_PB_STATE_STOP;
                    pvpb->pb.state = AX_PB_STATE_STOP;
                    __AXPushCallbackStack(pvpb);
                }
            } else {
                __AXServiceVPB(pvpb);
            }
            pvpb->sync = 0;
            pvpb->depop = FALSE;
            pvpb->updateCounter = 0;
            pvpb->updateMS = 0;
            pvpb->updateWrite = pvpb->updateData;
        }
    }
    __AXRecDspCycles = cycles;
    for (pvpb = __AXGetStackHead(AX_PRIORITY_FREE); pvpb != NULL; pvpb = (AXVPB*)pvpb->next) {
        if (pvpb->depop) {
            __AXDepopVoice(&__AXPB[pvpb->index]);
        }
        pvpb->depop = FALSE;
        __AXPB[pvpb->index].update.updNum[4] = 0;
        __AXPB[pvpb->index].update.updNum[3] = 0;
        __AXPB[pvpb->index].update.updNum[2] = 0;
        __AXPB[pvpb->index].update.updNum[1] = 0;
        __AXPB[pvpb->index].update.updNum[0] = 0;
        __AXPB[pvpb->index].state = AX_PB_STATE_STOP;
    }
    DCFlushRange(__AXPB, sizeof(__AXPB));
    DCFlushRange(__AXITD, sizeof(__AXITD));
    DCFlushRange(__AXUpdates, sizeof(__AXUpdates));
}

AXPB* __AXGetPBs(void) {
    return __AXPB;
}

void __AXSetPBDefault(AXVPB* p) {
    p->pb.state = AX_PB_STATE_STOP;
    p->pb.itd.flag = 0;
    p->sync = AX_SYNC_FLAG_COPYSTATE | AX_SYNC_FLAG_COPYITD | AX_SYNC_FLAG_COPYUPDATE;
    p->updateCounter = 0;
    p->updateMS = 0;
    p->updateWrite = p->updateData;
    p->pb.update.updNum[4] = 0;
    p->pb.update.updNum[3] = 0;
    p->pb.update.updNum[2] = 0;
    p->pb.update.updNum[1] = 0;
    p->pb.update.updNum[0] = 0;
}

void __AXVPBInit(void) {
    u32 i;
    AXPB* ppb;
    AXVPB* pvpb;
    u32* pitd;
    u16* pupd;

    __AXMaxDspCycles = OS_BUS_CLOCK / 400;
    __AXRecDspCycles = 0;
    for (i = 0; i < sizeof(__AXPB) / 4; i++) {
        ((u32*)__AXPB)[i] = 0;
    }
    for (i = 0; i < sizeof(__AXITD) / 4; i++) {
        ((u32*)__AXITD)[i] = 0;
    }
    for (i = 0; i < sizeof(__AXVPB) / 4; i++) {
        ((u32*)__AXVPB)[i] = 0;
    }
    ppb = __AXPB;
    pitd = __AXITD[0];
    pupd = __AXUpdates[0];
    pvpb = __AXVPB;
    for (i = 0; i < AX_MAX_VOICES; i++) {
        pvpb->index = i;
        pvpb->updateWrite = pvpb->updateData;
        pvpb->itdBuffer = pitd;
        __AXSetPBDefault(pvpb);
        if (i == AX_MAX_VOICES - 1) {
            ppb->nextLo = 0;
            ppb->nextHi = 0;
            pvpb->pb.nextLo = 0;
            pvpb->pb.nextHi = 0;
        } else {
            u32 next = (u32)(ppb + 1);
            pvpb->pb.nextHi = next >> 16;
            pvpb->pb.nextLo = next;
            ppb->nextHi = next >> 16;
            ppb->nextLo = next;
        }
        pvpb->pb.currHi = (u32)ppb >> 16;
        pvpb->pb.currLo = (u32)ppb;
        ppb->currHi = (u32)ppb >> 16;
        ppb->currLo = (u32)ppb;
        pvpb->pb.itd.bufferHi = (u32)pitd >> 16;
        pvpb->pb.itd.bufferLo = (u32)pitd;
        ppb->itd.bufferHi = (u32)pitd >> 16;
        ppb->itd.bufferLo = (u32)pitd;
        pvpb->pb.update.dataHi = (u32)pupd >> 16;
        pvpb->pb.update.dataLo = (u32)pupd;
        ppb->update.dataHi = (u32)pupd >> 16;
        ppb->update.dataLo = (u32)pupd;
        pvpb->priority = 1;
        __AXPushFreeStack(pvpb);
        ppb++;
        pitd += 16;
        pupd += 128;
        pvpb++;
    }
    DCFlushRange(__AXPB, sizeof(__AXPB));
}

void __AXVPBQuit(void) {
}

void AXSetVoiceSrcType(AXVPB* p, u32 type) {
    BOOL old = OSDisableInterrupts();
    AXPB* pb = &p->pb;
    switch (type) {
    case AX_SRC_TYPE_NONE:
        pb->srcSelect = 2;
        break;
    case AX_SRC_TYPE_LINEAR:
        pb->srcSelect = 1;
        break;
    case AX_SRC_TYPE_4TAP_8K:
        pb->srcSelect = 0;
        pb->coefSelect = 0;
        break;
    case AX_SRC_TYPE_4TAP_12K:
        pb->srcSelect = 0;
        pb->coefSelect = 1;
        break;
    case AX_SRC_TYPE_4TAP_16K:
        pb->srcSelect = 0;
        pb->coefSelect = 2;
        break;
    }
    p->sync |= AX_SYNC_FLAG_COPYSELECT;
    OSRestoreInterrupts(old);
}

void AXSetVoiceState(AXVPB* p, u16 state) {
    BOOL old = OSDisableInterrupts();
    p->pb.state = state;
    p->sync |= AX_SYNC_FLAG_COPYSTATE;
    if (state == AX_PB_STATE_STOP) {
        p->depop = TRUE;
    }
    OSRestoreInterrupts(old);
}

void AXSetVoiceAddr(AXVPB* p, AXPBADDR* addr) {
    BOOL old;
    u32* dst = (u32*)&p->pb.addr;
    u32* src = (u32*)addr;
    old = OSDisableInterrupts();
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    switch (addr->format) {
    case 0:
        break;
    case 10:
        // 16-bit PCM: unity gain, no prediction.
        dst[4] = 0;
        dst[5] = 0;
        dst[6] = 0;
        dst[7] = 0;
        dst[8] = 0;
        dst[9] = 0;
        dst[10] = 0;
        dst[11] = 0;
        dst[12] = 0x08000000;
        dst[13] = 0;
        break;
    case 25:
        // 8-bit PCM.
        dst[4] = 0;
        dst[5] = 0;
        dst[6] = 0;
        dst[7] = 0;
        dst[8] = 0;
        dst[9] = 0;
        dst[10] = 0;
        dst[11] = 0;
        dst[12] = 0x01000000;
        dst[13] = 0;
        break;
    }
    p->sync &= ~(AX_SYNC_FLAG_COPYLOOP | AX_SYNC_FLAG_COPYLOOPADDR | AX_SYNC_FLAG_COPYENDADDR |
                 AX_SYNC_FLAG_COPYCURADDR);
    p->sync |= AX_SYNC_FLAG_COPYADDR | AX_SYNC_FLAG_COPYADPCM;
    OSRestoreInterrupts(old);
}

void AXSetVoiceAdpcm(AXVPB* p, AXPBADPCM* adpcm) {
    BOOL old;
    u32* dst = (u32*)&p->pb.adpcm;
    u32* src = (u32*)adpcm;
    old = OSDisableInterrupts();
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    dst[4] = src[4];
    dst[5] = src[5];
    dst[6] = src[6];
    dst[7] = src[7];
    dst[8] = src[8];
    dst[9] = src[9];
    p->sync |= AX_SYNC_FLAG_COPYADPCM;
    OSRestoreInterrupts(old);
}

void AXSetVoiceSrcRatio(AXVPB* p, f32 ratio) {
    BOOL old = OSDisableInterrupts();
    u32 r = (u32)(65536.0f * ratio);
    if (r > 0x40000) {
        r = 0x40000;
    }
    p->pb.src.ratioHi = r >> 16;
    p->pb.src.ratioLo = r;
    p->sync |= AX_SYNC_FLAG_COPYRATIO;
    OSRestoreInterrupts(old);
}

void AXSetVoiceAdpcmLoop(AXVPB* p, AXPBADPCMLOOP* adpcmloop) {
    BOOL old;
    u16* dst = (u16*)&p->pb.adpcmLoop;
    u16* src = (u16*)adpcmloop;
    old = OSDisableInterrupts();
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    p->sync |= AX_SYNC_FLAG_COPYADPCMLOOP;
    OSRestoreInterrupts(old);
}
