#include "ea/snd/snddrv.h"
#include "ea/snd/smix.h"
#include "ea/snd/spktplay.h"
#include <dolphin/ai.h>
#include <string.h>

SNDDRV snddrv;
static s32 everyother;

void SNDDRV_freenextframe();
void SNDDRV_mixvoicefree(int voice);
void SNDDRV_callbackdropvoice(void* p);
int SNDDRV_allocateaxvoice(int voice);
void SNDDRV_freeaxvoice(int voice, int mode);
void SNDDRV_dmcallback(u32 req);
void SNDDRV_audiocallback(void* data, void* context);
void SNDDRV_fillbufwithpackets(int player, SNDDRVPKTCHAN* pc, CHANPUB* v);
void SNDDRV_thread(void* param);
void SNDPLATFORM_lowpass(int voice, int freq);
void SNDPLATFORM_highpass(int voice, int freq);

static inline int numhw() {
    return sndgs.opts.caps.numHwVoices;
}

static inline CHANPUB* chanpub(int voice) {
    return &sndgs.voices[voice];
}

// Takes a free transfer slot (or reuses `slot`) and queues the transfer to ARAM.
static inline void postdma(int slot, u32 priority, u32 src, u32 dst, u32 len) {
    BOOL old = OSDisableInterrupts();
    u32 owner;
    if (slot == -1) {
        owner = snddrv.dmaId;
        BOOL old2 = OSDisableInterrupts();
        for (slot = 0; slot < 64; slot++) {
            if (snddrv.dma[slot].id == -1) {
                break;
            }
        }
        snddrv.dma[slot].id = owner;
        OSRestoreInterrupts(old2);
        owner = snddrv.dmaId;
        snddrv.dma[slot].priority = priority;
        snddrv.dma[slot].src = src;
        snddrv.dma[slot].dst = dst;
        snddrv.dma[slot].len = (len + 31) & ~31;
        snddrv.dma[slot].time = sndgs.serverTicks;
    } else {
        owner = snddrv.dma[slot].id;
    }
    ARQPostRequest(&snddrv.dma[slot].req, owner, ARQ_TYPE_MRAM_TO_ARAM, priority, src, dst, (len + 31) & ~31,
                   SNDDRV_dmcallback);
    OSRestoreInterrupts(old);
    snddrv.dmaId++;
}

// Frees the voices whose AX voice was dropped last frame.
void SNDDRV_freenextframe() {
    SNDDRVVOICE* d = snddrv.voices;
    if (snddrv.numFreePending == 0) {
        return;
    }
    for (int i = 0; i < 61; i++, d++) {
        if (d->freeNextFrame) {
            AXFreeVoice(d->axvpb);
            d->freeNextFrame = 0;
            snddrv.numFreePending--;
            d->axvpb = NULL;
            if (snddrv.numFreePending == 0) {
                break;
            }
        }
    }
}

// A CPU mixer voice's source ended.
void SNDDRV_mixvoicefree(int voice) {
    SNDVOICEI_free(voice + numhw());
}

// AX dropped a voice (stolen by a higher priority): release the sound's other channels too.
void SNDDRV_callbackdropvoice(void* p) {
    AXVPB* axvpb = (AXVPB*)p;
    int voice = axvpb->userContext;
    s16 master = chanpub(voice)->master;
    if (master == -1) {
        master = voice;
    }
    CHANPUB* v = chanpub(master);
    for (int i = 0; i < v->channels; i++) {
        int ch = v->chanVoice[i];
        if (snddrv.voices[ch].axvpb != NULL) {
            if (ch == (int)axvpb->userContext) {
                SNDDRV_freeaxvoice(ch, 1);
            } else {
                SNDDRV_freeaxvoice(ch, 2);
            }
            snddrv.voices[ch].active = 0;
        }
    }
}

// Gets AX voices for each channel of the sound; on failure frees what it got and the sound.
int SNDDRV_allocateaxvoice(int voice) {
    CHANPUB* v = chanpub(voice);
    int i;
    for (i = 0; i < v->channels; i++) {
        int priority;
        if (v->priority == 101) {
            priority = AX_PRIORITY_STACKS - 1;
        } else {
            priority = v->priority / 3;
        }
        if (priority <= 0) {
            priority = 1;
        } else if (priority > AX_PRIORITY_STACKS - 1) {
            priority = AX_PRIORITY_STACKS - 2;
        }
        SNDDRVVOICE* d = &snddrv.voices[v->chanVoice[i]];
        d->axvpb = AXAcquireVoice(priority, SNDDRV_callbackdropvoice, v->chanVoice[i]);
        if (d->axvpb == NULL) {
            snddrv.voices[v->chanVoice[i]].noVoice = 1;
            if (i > 0) {
                for (; i >= 0; i--) {
                    SNDDRVVOICE* f = &snddrv.voices[v->chanVoice[i]];
                    if (f->axvpb != NULL) {
                        AXFreeVoice(f->axvpb);
                        f->noVoice = 1;
                        f->axvpb = NULL;
                    }
                    if ((s8)chanpub(v->chanVoice[i])->state != 0) {
                        SNDVOICEI_free(v->chanVoice[i]);
                    }
                }
            }
            return 0;
        }
        d->noVoice = 0;
        SNDDRV_DSPMixerInitChannel(snddrv.voices[v->chanVoice[i]].axvpb, 0, 0, 0, 0);
    }
    return 1;
}

// mode 0: free the AX voice now; 1: AX dropped it; 2: free it next frame.
void SNDDRV_freeaxvoice(int voice, int mode) {
    SNDDRVVOICE* d = &snddrv.voices[voice];
    AXVPB* p = d->axvpb;
    CHANPUB* v = chanpub(voice);
    SNDDRV_DSPMixerReleaseChannel(p);
    d->base = -1;
    if ((s8)v->state != 0) {
        SNDVOICEI_free(voice);
    }
    switch (mode) {
    case 0:
        if (p != NULL) {
            AXFreeVoice(p);
            d->axvpb = NULL;
        }
        break;
    case 2:
        if (d->noVoice == 0 && p != NULL) {
            d->freeNextFrame = 1;
            snddrv.numFreePending++;
        }
        break;
    case 1:
        if (d->freeNextFrame == 1) {
            d->freeNextFrame = 0;
            snddrv.numFreePending--;
        }
        d->axvpb = NULL;
        break;
    }
    if (d->noVoice == 0) {
        d->noVoice = 1;
    }
}

void SNDDRV_dmcallback(u32 req) {
    ARQRequest* r = ADDR_TO_PTR(ARQRequest, req);
    snddrv.lastDone = r->owner;
    for (int i = 0; i < 64; i++) {
        if (snddrv.dma[i].id == (s32)r->owner) {
            snddrv.dma[i].id = -1;
            return;
        }
    }
}

// AX aux A callback: hands the reverb send to the CPU mixer and returns the CPU mix to the DSP.
void SNDDRV_audiocallback(void* data, void* context) {
    static f32* ptr;
    s32** bufs = (s32**)data;
    BOOL old = OSEnableInterrupts();
    if (snddrv.mixerInited) {
        f32* wet = MIX_getwetbuffer();
        ptr = wet;
        s32* src = bufs[0];
        for (int i = 0; i < 160; i++) {
            wet[i] = src[i];
        }
        SNDI_memcpy(bufs[1], snddrv.mixOut[0], 0x280);
        if (sndgs.opts.caps.speakers < 3) {
            SNDI_memcpy(bufs[0], snddrv.mixOut[1], 0x280);
        } else {
            SNDI_memcpy(bufs[0], snddrv.mixOut[2], 0x280);
            SNDI_memcpy(bufs[2], snddrv.mixOut[1], 0x280);
        }
    }
    OSWakeupThread(&snddrv.queue);
    everyother++;
    OSRestoreInterrupts(old);
}

void SNDDRV_initmixer() {
    MIXOPTS opts;
    SNDSYS_linkmaincpumixer();
    opts.numVoices = sndgs.opts.caps.numSwVoices;
    if (sndgs.opts.caps.speakers != 1) {
        opts.numOutputs = sndgs.opts.caps.speakers;
    } else {
        opts.numOutputs = 2;
    }
    opts.rate = sndgs.opts.caps.rate;
    opts.stopcb = SNDDRV_mixvoicefree;
    opts.unk_0x6 = 0;
    MIXinitfn(&opts);
    for (int i = 0; i < opts.numOutputs; i++) {
        SNDSYS_entercritical();
        snddrv.mixMem[i] = SNDMEMI_allocz(0x2A0);
        snddrv.mixOut[i] = (f32*)(((uptr)(snddrv.mixMem[i]) + 31) & ~31);
        SNDSYS_leavecritical();
    }
    snddrv.mixerInited = 1;
}

// Refills the ring half that just finished playing from the packet queue (or silence).
void SNDDRV_fillbufwithpackets(int player, SNDDRVPKTCHAN* pc, CHANPUB* v) {
    u8* src[3];
    int samples;
    int unused;
    int left = 0x600;
    int ch;

    while (left > 0) {
        if (pc->pkt[0] == NULL) {
            for (ch = 0; ch < v->channels; ch++) {
                pc->pkt[ch] = ADDR_TO_PTR(u8, SNDPKTPLAYI_get(player, ch, &samples, &unused));
            }
            pc->pos = 0;
            pc->bytes = samples * 2;
        }
        int n = left;
        if (pc->pkt[0] != NULL) {
            int avail = pc->bytes - pc->pos;
            if (avail <= 0) {
                pc->pkt[0] = NULL;
                continue;
            }
            if (left > avail) {
                n = avail;
            }
            pc->consumed[pc->half] += n >> 1;
            for (ch = 0; ch < v->channels; ch++) {
                src[ch] = pc->pkt[ch] + pc->pos;
            }
            pc->pos += n;
        } else {
            for (ch = 0; ch < v->channels; ch++) {
                src[ch] = snddrv.zeroBuf;
            }
        }
        for (ch = 0; ch < v->channels; ch++) {
            SNDI_memcpy(pc->bounce[ch] + (0x600 - left), src[ch], n);
        }
        left -= n;
    }
    for (ch = 0; ch < v->channels; ch++) {
        DCFlushRange(pc->bounce[ch], 0x600);
        postdma(-1, ARQ_PRIORITY_HIGH, (u32)PTR_TO_ADDR(pc->bounce[ch]), pc->aram[ch] + pc->half * 0x600, 0x600);
    }
}

// Mixes a CPU slice per DSP frame and runs the 100 Hz server every other frame.
void SNDDRV_thread(void* param) {
    for (;;) {
        SNDI_mutexlock();
        if (sndgs.premix != NULL) {
            sndgs.premix();
        }
        if (snddrv.mixerInited) {
            MIXaudioslicefn(snddrv.mixOut, 160);
            SNDPKTPLAYI_flushcallbackdata();
        }
        if (everyother & 1) {
            SNDSYSI_100hzserver();
        }
        if (sndgs.postmix != NULL) {
            sndgs.postmix();
        }
        SNDI_mutexunlock();
        OSSleepThread(&snddrv.queue);
    }
}

int SNDPLATFORM_outputcaps() {
    SNDSYSOPTS* o = &sndgs.opts;
    o->hw.maxSwVoices = 16;
    o->hw.maxHwVoices = 61;
    o->hw.unk_0x0 = 32000;
    o->hw.unk_0x2 = 32000;
    o->hw.minSpeakers = 1;
    o->hw.maxSpeakers = 3;
    o->caps.unk_0x29[1] = 1;
    o->hw.numRenderModes = 2;
    o->hw.renderModes[0] = 0x220;
    o->hw.renderModes[1] = 0x24;
    o->caps.numRenderModes = 2;
    o->caps.renderModes[0] = 0x220;
    o->caps.renderModes[1] = 0x24;
    o->caps.numSwVoices = 8;
    o->caps.numHwVoices = 48;
    o->caps.maxPacketPlayers = 8;
    o->caps.rate = 32000;
    o->caps.speakers = 2;
    o->caps.initAram = 1;
    u32 base = ARGetBaseAddress() + 0x100;
    o->caps.aramBase = base;
    o->caps.aramSize = 0x800000 - base;
    o->caps.unk_0x14 = 10;
    *(u32*)&o->caps.speakerAz[0][0] = 0;
    o->caps.speakerAz[1][0] = 0x4000;
    o->caps.speakerAz[1][1] = 0xC000;
    o->caps.speakerAz[2][0] = 0x4000;
    o->caps.speakerAz[2][1] = 0x8000;
    o->caps.speakerAz[2][2] = 0xC000;
    return 0;
}

// Clamps the requested setup to the hardware. Once running only the current setup is kept.
int SNDPLATFORM_outputset() {
    SNDSYSCAPS caps = sndgs.opts.caps;
    if (sndgs.inited) {
        sndgs.opts.caps = sndgs.caps;
    }
    caps.rate = 32000;
    if (caps.speakers < sndgs.opts.hw.minSpeakers) {
        caps.speakers = sndgs.opts.hw.minSpeakers;
    }
    if (caps.speakers > sndgs.opts.hw.maxSpeakers) {
        caps.speakers = sndgs.opts.hw.maxSpeakers;
    }
    if (caps.numSwVoices > sndgs.opts.hw.maxSwVoices) {
        caps.numSwVoices = sndgs.opts.hw.maxSwVoices;
    }
    if (caps.numHwVoices > sndgs.opts.hw.maxHwVoices) {
        caps.numHwVoices = sndgs.opts.hw.maxHwVoices;
    }
    if (caps.maxPacketPlayers > 32) {
        caps.maxPacketPlayers = 32;
    }
    u8 n = sndgs.opts.caps.unk_0x14;
    if (n < 1) {
        n = 1;
    } else if (n > 200) {
        n = 200;
    }
    sndgs.opts.caps.unk_0x14 = n;
    if (sndgs.inited) {
        return 0;
    }
    sndgs.numVoices = caps.numSwVoices + caps.numHwVoices;
    sndgs.opts.caps = caps;
    return 0;
}

int SNDPLATFORM_init() {
    static int araminited = 0;
    int i;

    SNDDRV_DSPMixerInit(sndgs.opts.caps.speakers);
    if (sndgs.opts.caps.initAram && !araminited) {
        ARInit(NULL, 0);
        ARQInit();
        araminited = 1;
    }
    for (i = 0; i < 64; i++) {
        snddrv.dma[i].id = -1;
    }
    if (sndgs.opts.aramalloc == NULL || sndgs.opts.aramfree == NULL) {
        SNDARAM_init(sndgs.opts.caps.maxBanks + 32);
        SNDARAM_setpool(sndgs.opts.caps.aramBase, sndgs.opts.caps.aramSize);
    }
    u32 zero = (u32)PTR_TO_ADDR(SNDPLATFORM_memalloc(0, 0x500));
    u8* buf = (u8*)(((uptr)(snddrv.zeroMem) + 31) & ~31);
    snddrv.zeroAram = zero;
    snddrv.zeroAram16 = zero >> 1;
    snddrv.zeroAramNibble = zero * 2;
    snddrv.zeroBuf = buf;
    postdma(-1, ARQ_PRIORITY_HIGH, (u32)PTR_TO_ADDR(buf), zero, 0x500);
    postdma(-1, ARQ_PRIORITY_HIGH, (u32)PTR_TO_ADDR(snddrv.zeroBuf), ARGetBaseAddress(), 0x100);
    AIInit(NULL);
    AXInit();
    if (sndgs.opts.caps.speakers == 3) {
        AXSetMode(AX_MODE_SURROUND);
    } else {
        AXSetMode(AX_MODE_STEREO);
    }
    for (i = 0; i < 61; i++) {
        SNDDRVVOICE* d = &snddrv.voices[i];
        d->base = -1;
        d->pktPlayer = -1;
        d->axvpb = NULL;
        d->active = 0;
        d->noVoice = 1;
        d->freeNextFrame = 0;
    }
    for (i = 0; i < sndgs.opts.caps.numRenderModes; i++) {
        if ((sndgs.opts.caps.renderModes[i] & 4) && !snddrv.mixerInited) {
            SNDDRV_initmixer();
            break;
        }
    }
    snddrv.numFreePending = 0;
    OSCreateThread(&snddrv.thread, (void* (*)(void*))SNDDRV_thread, NULL, snddrv.stack + sizeof(snddrv.stack),
                   sizeof(snddrv.stack), 1, 1);
    OSInitThreadQueue(&snddrv.queue);
    OSResumeThread(&snddrv.thread);
    AXRegisterAuxACallback(SNDDRV_audiocallback, NULL);
    sndgs.inited = 1;
    SNDSYS_add100hzclient(SNDDRV_freenextframe);
    SNDSYS_add100hzclient(SNDDRV_DSPMixerUpdate);
    return 0;
}

int SNDPLATFORM_restore() {
    if (snddrv.fxcb != NULL) {
        snddrv.fxcb(4);
    }
    AXRegisterAuxACallback(NULL, NULL);
    if (snddrv.mixerInited) {
        snddrv.mixerInited = 0;
        SNDSYS_entercritical();
        for (int i = 0; i < sndgs.opts.caps.speakers; i++) {
            SNDMEMI_free(snddrv.mixMem[i]);
        }
        SNDSYS_leavecritical();
        MIXrestorefn();
    }
    SNDARAM_restore();
    while (sndgs.critical) {
        SNDSYS_leavecritical();
    }
    OSCancelThread(&snddrv.thread);
    SNDSYS_remove100hzclient(SNDDRV_freenextframe);
    SNDSYS_remove100hzclient(SNDDRV_DSPMixerUpdate);
    AXQuit();
    return 0;
}

// Starts packet players on the DSP and refills their rings; frees DSP voices that stopped.
void iSNDserve() {
    for (int i = 0; i < sndgs.opts.caps.maxPacketPlayers; i++) {
        SNDDRVPKTCHAN* pc = snddrv.pkt[i];
        if (pc == NULL || pc->voice < 0) {
            continue;
        }
        CHANPUB* v = chanpub(pc->voice);
        if (pc->startDelay >= 0) {
            pc->startDelay--;
            if (pc->startDelay < 0) {
                AXPBADPCM adpcm;
                AXPBADPCMLOOP loop;
                AXPBADDR addr;
                addr.loopFlag = 1;
                addr.format = 10;
                adpcm.gain = 0x800;
                memset(&loop, 0, sizeof(loop));
                SNDPLATFORM_setpitch(pc->voice);
                SNDPLATFORM_set3dpos(pc->voice);
                SNDPLATFORM_setvol(pc->voice);
                for (int ch = 0; ch < v->channels; ch++) {
                    u32 start = pc->aram[ch] >> 1;
                    u32 end = (pc->aram[ch] + 0xBFE) >> 1;
                    AXSetVoiceAdpcm(snddrv.voices[v->chanVoice[ch]].axvpb, &adpcm);
                    AXSetVoiceAdpcmLoop(snddrv.voices[v->chanVoice[ch]].axvpb, &loop);
                    addr.endAddressHi = end >> 16;
                    addr.endAddressLo = end;
                    addr.currentAddressHi = start >> 16;
                    addr.currentAddressLo = start;
                    addr.loopAddressHi = start >> 16;
                    addr.loopAddressLo = start;
                    AXSetVoiceAddr(snddrv.voices[v->chanVoice[ch]].axvpb, &addr);
                    AXSetVoiceState(snddrv.voices[v->chanVoice[ch]].axvpb, AX_PB_STATE_RUN);
                    snddrv.voices[v->chanVoice[ch]].active = 1;
                }
            }
        }
        AXVPB* p = snddrv.voices[pc->voice].axvpb;
        u32 cur = (p->pb.addr.currentAddressHi << 16) + p->pb.addr.currentAddressLo;
        u8 half = pc->halfMark < cur;
        if (half != pc->half) {
            // The other half started playing: free what was played and refill it.
            if (pc->consumed[pc->half] != 0) {
                for (int ch = 0; ch < v->channels; ch++) {
                    SNDPKTPLAYI_freeframes(i, ch, pc->consumed[pc->half]);
                }
                pc->consumed[pc->half] = 0;
            }
            SNDDRV_fillbufwithpackets(i, pc, v);
            pc->half = half;
        }
    }
    SNDPKTPLAYI_flushcallbackdata();
    for (int i = 0; i < numhw(); i++) {
        SNDDRVVOICE* d = &snddrv.voices[i];
        CHANPUB* v = chanpub(i);
        AXVPB* p = d->axvpb;
        if (p != NULL && d->active && p->pb.state == AX_PB_STATE_STOP && d->base >= 0 && d->active == 1) {
            for (int ch = 0; ch < chanpub(i)->channels; ch++) {
                if (snddrv.voices[v->chanVoice[ch]].active) {
                    SNDDRV_freeaxvoice(v->chanVoice[ch], 0);
                    snddrv.voices[v->chanVoice[ch]].active = 0;
                }
            }
        }
    }
}

// renderMode 0x200: DSP voices, 4: CPU mixer voices.
void SNDPLATFORM_getvoicerange(int renderMode, int* lo, int* hi) {
    if (renderMode & 0x200) {
        *lo = 0;
        *hi = numhw();
        return;
    }
    if (renderMode & 4) {
        *lo = numhw();
        *hi = sndgs.numVoices;
    }
}

// Points the timbre's ARAM address at where its bank's samples were put.
int SNDPLATFORM_asyncresolvetimbre(SNDIPATCHHEADER* header, char* aram, int* state) {
    if (*state == 0) {
        *state = header->offset[0];
    }
    *header->aramAddr = (s32)PTR_TO_ADDR(aram) + (header->offset[0] - *state);
    return 0;
}

int SNDPLATFORM_playtimbre(SNDIPATCHHEADER* header, void* bank, int voice, int timemult, int a, int lowpass,
                           int highpass) {
    CHANPUB* v = chanpub(voice);
    u32 stride = 0;
    u32 base = 0;
    v->rate = header->rate;
    v->endFrame = header->samples;
    v->channels = header->channels;
    if (v->renderMode & 0x200) {
        AXPBADPCMLOOP loop;
        AXPBADDR addr;
        AXPBADPCM adpcm;
        u32 loopStart;
        u32 end;
        memset(&loop, 0, sizeof(loop));
        if (header->loopEnd >= 0) {
            end = header->loopEnd;
            addr.loopFlag = 1;
            loopStart = header->loopStart;
        } else {
            addr.loopFlag = 0;
            if (v->unk_0x1E == 0x12) {
                loopStart = snddrv.zeroAramNibble;
            } else {
                loopStart = snddrv.zeroAram16;
            }
            end = header->samples - 1;
        }
        if (v->unk_0x1E == 0x12) {
            // DSP ADPCM: 14 samples in 16 nibbles.
            addr.format = 0;
            adpcm.gain = 0;
            adpcm.yn1 = 0;
            adpcm.yn2 = 0;
            loopStart = (loopStart * 8) / 7;
            end = (end * 8) / 7;
            base = *header->aramAddr * 2;
            stride = ((u32)(header->samples + 13) / 14 * 14 * 8) / 7;
        } else if (v->unk_0x1E == 7) {
            // 16-bit PCM.
            addr.format = 10;
            adpcm.gain = 0x800;
            stride = header->samples;
            base = (u32)*header->aramAddr >> 1;
        }
        snddrv.voices[voice].base = base;
        if (SNDDRV_allocateaxvoice(voice) == 0) {
            return -9;
        }
        SNDPLATFORM_set3dpos(voice);
        SNDPLATFORM_setvol(voice);
        SNDPLATFORM_setpitch(voice);
        SNDPLATFORM_setfxlevel(v->chanVoice[0], 0);
        for (int i = 0; i < (s8)header->channels; i++) {
            if (v->unk_0x1E == 0x12) {
                SNDI_memcpy(adpcm.a, header->adpcm[i], 0x20);
                adpcm.pred_scale = ((u8*)header->adpcm[i])[0x20];
                if (header->loopEnd > 0) {
                    u8* ctx = (u8*)header->adpcmLoop[i];
                    loop.loop_pred_scale = ctx[4];
                    loop.loop_yn1 = ((u16*)ctx)[0];
                    loop.loop_yn2 = ((u16*)ctx)[1];
                }
            }
            u32 cur = base + stride * i;
            u32 loopAddr = loopStart;
            if (header->loopEnd > 0) {
                loopAddr = cur + loopStart;
            }
            u32 endAddr = cur + end - 2;
            if (v->unk_0x1E == 0x12) {
                // Skip the frame header nibbles.
                cur += 2;
                if ((loopAddr & 0xF) == 0) {
                    loopAddr += 2;
                } else if ((loopAddr & 0xF) == 1) {
                    loopAddr += 1;
                }
                if ((endAddr & 0xF) == 0) {
                    endAddr += 2;
                } else if ((endAddr & 0xF) == 1) {
                    endAddr += 1;
                }
            }
            AXVPB* p = snddrv.voices[v->chanVoice[i]].axvpb;
            AXSetVoiceSrcType(p, AX_SRC_TYPE_LINEAR);
            AXSetVoiceAdpcm(snddrv.voices[v->chanVoice[i]].axvpb, &adpcm);
            AXSetVoiceAdpcmLoop(snddrv.voices[v->chanVoice[i]].axvpb, &loop);
            addr.endAddressHi = endAddr >> 16;
            addr.endAddressLo = endAddr;
            addr.currentAddressHi = cur >> 16;
            addr.currentAddressLo = cur;
            addr.loopAddressHi = loopAddr >> 16;
            addr.loopAddressLo = loopAddr;
            AXSetVoiceAddr(snddrv.voices[v->chanVoice[i]].axvpb, &addr);
            AXSetVoiceState(snddrv.voices[v->chanVoice[i]].axvpb, AX_PB_STATE_RUN);
            snddrv.voices[v->chanVoice[i]].active = 1;
        }
        return 0;
    }
    for (int i = 0; i < v->channels; i++) {
        MIX_playinit(v->chanVoice[i] - numhw(), header->unk_0x13, 1, (int)PTR_TO_ADDR(bank) + header->offset[i], -1,
                     (u8*)header->data[i], 0, v->endFrame, header->loopStart, header->loopEnd, 0, 0, i);
    }
    SNDPLATFORM_setpitch(v->chanVoice[0]);
    SNDPLATFORM_set3dpos(v->chanVoice[0]);
    SNDPLATFORM_setfxlevel(v->chanVoice[0], 0);
    SNDPLATFORM_timemult(v->chanVoice[0], timemult);
    SNDPLATFORM_lowpass(v->chanVoice[0], lowpass);
    SNDPLATFORM_highpass(v->chanVoice[0], highpass);
    for (int i = 0; i < v->channels; i++) {
        MIX_play(v->chanVoice[i] - numhw());
    }
    return 0;
}

int SNDDRV_getmastervoice(int voice) {
    s16 v = voice + numhw();
    s16 master = chanpub(v)->master;
    if (master == -1) {
        return v;
    }
    return master;
}

// Which channel of its sound a CPU voice is.
int SNDDRV_getsamplechan(int voice) {
    int v = voice + numhw();
    s16 master = chanpub(v)->master;
    if (master == -1) {
        return 0;
    }
    CHANPUB* m = chanpub(master);
    if (m->chanVoice[1] == v) {
        return 1;
    }
    if (m->chanVoice[2] == v) {
        return 2;
    }
    if (m->chanVoice[3] == v) {
        return 3;
    }
    return 0;
}

int SNDPLATFORM_packetoverhead() {
    return sizeof(SNDDRVPKTCHAN);
}

int SNDPLATFORM_packetplaycreate(int player, void* buffer) {
    SNDDRVPKTCHAN* pc = (SNDDRVPKTCHAN*)buffer;
    memset(pc, 0, sizeof(SNDDRVPKTCHAN));
    pc->aram[0] = (u32)PTR_TO_ADDR(SNDPLATFORM_memalloc(0, 0xC00));
    if (pc->aram[0] == 0) {
        return -9;
    }
    pc->aram[1] = (u32)PTR_TO_ADDR(SNDPLATFORM_memalloc(0, 0xC00));
    if (pc->aram[1] == 0) {
        return -9;
    }
    pc->bounce[0] = (u8*)(((uptr)(pc) + 0x1F) & ~0x1F);
    pc->bounce[1] = (u8*)(((uptr)(pc) + 0x65F) & ~0x1F);
    pc->bounce[2] = (u8*)(((uptr)(pc) + 0xC9F) & ~0x1F);
    pc->voice = -1;
    snddrv.pkt[player] = pc;
    return 0;
}

int SNDPLATFORM_packetplay(int player, int voice, int timemult, int a, int lowpass, int highpass,
                           SNDSAMPLEFORMAT* format, void** data) {
    CHANPUB* v = chanpub(voice);
    v->rate = format->rate;
    v->endFrame = 0;
    snddrv.voices[voice].pktPlayer = player;
    v->channels = format->channels;
    if (v->renderMode & 4) {
        for (int i = 0; i < v->channels; i++) {
            MIXplayinitfn(v->chanVoice[i] - numhw(), v->unk_0x1E, 0, 0, -1, (u8*)data[i], 0, 0, -1, -1, 0, 0, i);
        }
        SNDPLATFORM_setpitch(v->chanVoice[0]);
        SNDPLATFORM_set3dpos(v->chanVoice[0]);
        SNDPLATFORM_setvol(v->chanVoice[0]);
        SNDPLATFORM_setfxlevel(v->chanVoice[0], 0);
        SNDPLATFORM_timemult(v->chanVoice[0], timemult);
        SNDPLATFORM_lowpass(v->chanVoice[0], lowpass);
        SNDPLATFORM_highpass(v->chanVoice[0], highpass);
        for (int i = 0; i < v->channels; i++) {
            MIXplayfn(v->chanVoice[i] - numhw());
        }
        return 0;
    }
    SNDDRVPKTCHAN* pc = snddrv.pkt[player];
    pc->voice = voice;
    pc->consumed[0] = 0;
    pc->consumed[1] = 0;
    pc->pkt[0] = NULL;
    pc->startDelay = 2;
    pc->half = 1;
    SNDDRV_allocateaxvoice(voice);
    for (int i = 0; i < format->channels; i++) {
        u32 aram = pc->aram[i];
        if (i == 0) {
            pc->halfMark = ((aram >> 1) + ((aram + 0xBFE) >> 1)) >> 1;
        }
        memset(pc->bounce[i], 0, 0x600);
        DCFlushRange(pc->bounce[i], 0x600);
        postdma(-1, ARQ_PRIORITY_HIGH, (u32)PTR_TO_ADDR(pc->bounce[i]), pc->aram[i], 0x600);
        AXSetVoiceSrcType(snddrv.voices[v->chanVoice[i]].axvpb, AX_SRC_TYPE_LINEAR);
        snddrv.voices[v->chanVoice[i]].active = 1;
    }
    return 0;
}

int SNDPLATFORM_packetplaydestroy(int player) {
    SNDDRVPKTCHAN* pc = snddrv.pkt[player];
    SNDPLATFORM_memfree(0, pc->aram[0]);
    SNDPLATFORM_memfree(0, pc->aram[1]);
    snddrv.pkt[player] = NULL;
    return 0;
}

int SNDPLATFORM_outputlatency() {
    return -15;
}

void SNDPLATFORM_setvol(int voice) {
    CHANPUB* v = chanpub(voice);
    if (v->renderMode & 0x200) {
        int level = (int)(0.007874016f * ((s8)v->unk_0x45 * (s8)v->unk_0x53)) << 8;
        for (int i = 0; i < v->channels; i++) {
            SNDDRV_DSPMixerSetVol(snddrv.voices[v->chanVoice[i]].axvpb, level);
        }
        return;
    }
    u8 outputs = 2;
    f32 gain = 1.8921513e-9f * ((s8)v->unk_0x45 * (s8)v->unk_0x53);
    if (sndgs.opts.caps.speakers != 1) {
        outputs = sndgs.opts.caps.speakers;
    }
    for (int i = 0; i < v->channels; i++) {
        s16* vols = snddrv.voices[v->chanVoice[i]].vols;
        for (int j = 0; j < outputs; j++) {
            SNDMIX_setdrygain(v->chanVoice[i] - numhw(), j, gain * *vols++);
        }
    }
}

void SNDPLATFORM_setdrylevel(int voice) {
    u16 mode = chanpub(voice)->renderMode;
    if ((mode & 0x200) || (mode & 4)) {
        SNDPLATFORM_setvol(voice);
    }
}

void SNDPLATFORM_set3dpos(int voice) {
    CHANPUB* v = chanpub(voice);
    int i;
    if (sndgs.opts.caps.speakers == 1) {
        for (i = 0; i < v->channels; i++) {
            snddrv.voices[v->chanVoice[i]].vols[0] = 0x6000;
            snddrv.voices[v->chanVoice[i]].vols[1] = 0x6000;
        }
        if (v->renderMode & 0x200) {
            for (i = 0; i < v->channels; i++) {
                SNDDRV_DSPMixerSetPan(snddrv.voices[v->chanVoice[i]].axvpb,
                                      (u16*)snddrv.voices[v->chanVoice[i]].vols);
            }
            return;
        }
        SNDPLATFORM_setvol(voice);
        return;
    }
    if (v->channels == 1) {
        SNDI_aztospkrvol(v->azimuth, snddrv.voices[voice].vols);
    } else {
        for (i = 0; i < v->channels; i++) {
            SNDI_aztospkrvol(v->chanAzimuth[i], snddrv.voices[v->chanVoice[i]].vols);
        }
    }
    if (v->renderMode & 0x200) {
        if (sndgs.opts.caps.speakers == 3) {
            // Turn down the front speakers as the sound moves to the surround.
            for (i = 0; i < 3; i++) {
                s16* vols = snddrv.voices[v->chanVoice[i]].vols;
                if (vols[1] > 15000) {
                    f32 k = 1.0 - (0.5f * vols[1]) / 100000.0f;
                    vols[0] = vols[0] * k;
                    vols = snddrv.voices[v->chanVoice[i]].vols;
                    vols[2] = vols[2] * (1.0f - (0.5f * vols[1]) / 100000.0f);
                }
            }
        }
        for (i = 0; i < v->channels; i++) {
            SNDDRV_DSPMixerSetPan(snddrv.voices[v->chanVoice[i]].axvpb, (u16*)snddrv.voices[v->chanVoice[i]].vols);
        }
        return;
    }
    SNDPLATFORM_setvol(voice);
}

int SNDPLATFORM_setpitch(int voice) {
    CHANPUB* v = chanpub(voice);
    int i;
    if (v->renderMode & 0x200) {
        f32 ratio = 0.00024414062f * ((u32)(((v->rate << 12) / sndgs.opts.caps.rate) * v->pitchMult2) >> 12);
        for (i = 0; i < v->channels; i++) {
            if (snddrv.voices[v->chanVoice[i]].axvpb == NULL) {
                return -8;
            }
            AXSetVoiceSrcRatio(snddrv.voices[v->chanVoice[i]].axvpb, ratio);
        }
        return 0;
    }
    u32 pitch = (u32)(((v->rate << 15) / sndgs.opts.caps.rate) * v->pitchMult2) >> 11;
    for (i = 0; i < v->channels; i++) {
        MIXsetpitchfn(v->chanVoice[i] - numhw(), pitch);
    }
    return 0;
}

int SNDPLATFORM_timemult(int voice, int mult) {
    CHANPUB* v = chanpub(voice);
    if (v->renderMode & 4) {
        for (int i = 0; i < v->channels; i++) {
            MIX_settimemult(v->chanVoice[i] - numhw(), mult);
        }
    }
    return 0;
}

// Note: the cutoff fraction is truncated to an integer, so any cutoff below the Nyquist rate
// becomes 0.
void SNDPLATFORM_lowpass(int voice, int freq) {
    CHANPUB* v = chanpub(voice);
    if (v->renderMode & 4) {
        int frac = (int)(freq * (1.0f / (0.5f * sndgs.opts.caps.rate)));
        for (int i = 0; i < v->channels; i++) {
            MIX_setlowpass(v->chanVoice[i] - numhw(), frac);
        }
    }
}

void SNDPLATFORM_highpass(int voice, int freq) {
    CHANPUB* v = chanpub(voice);
    if (v->renderMode & 4) {
        for (int i = 0; i < v->channels; i++) {
            MIX_sethighpass(v->chanVoice[i] - numhw(), freq);
        }
    }
}

int SNDPLATFORM_getcurframe(int voice) {
    CHANPUB* v = chanpub(voice);
    SNDDRVVOICE* d = &snddrv.voices[voice];
    if (v->renderMode & 0x200) {
        AXVPB* p = d->axvpb;
        if (p == NULL) {
            return -8;
        }
        int cur = (p->pb.addr.currentAddressHi << 16) + p->pb.addr.currentAddressLo;
        if (v->unk_0x1E == 0x12) {
            return ((cur - d->base) * 7) >> 3;
        }
        return cur - d->base;
    }
    return MIX_getframe(voice - numhw());
}

int SNDPLATFORM_stop(int voice) {
    CHANPUB* v = chanpub(voice);
    SNDDRVVOICE* d = &snddrv.voices[voice];
    int i;
    if (v->renderMode & 0x200) {
        s8 player = d->pktPlayer;
        if (player >= 0) {
            snddrv.pkt[player]->voice = -1;
            d->pktPlayer = -1;
        }
        for (i = 0; i < v->channels; i++) {
            if (snddrv.voices[v->chanVoice[i]].axvpb == NULL) {
                return -8;
            }
            AXSetVoiceState(snddrv.voices[v->chanVoice[i]].axvpb, AX_PB_STATE_STOP);
            if (snddrv.voices[v->chanVoice[i]].active) {
                SNDDRV_freeaxvoice(v->chanVoice[i], 0);
                snddrv.voices[v->chanVoice[i]].active = 0;
            }
        }
        return 0;
    }
    for (i = 0; i < v->channels; i++) {
        MIXstopfn(v->chanVoice[i] - numhw());
        SNDVOICEI_free(v->chanVoice[i]);
    }
    return 0;
}

int SNDPLATFORM_filteradd(int voice, SNDFILTERDEF* filter) {
    return -15;
}

void* SNDPLATFORM_memalloc(int type, int size) {
    size = (size + 31) & ~31;
    if (sndgs.opts.aramalloc != NULL) {
        return ADDR_TO_PTR(void, sndgs.opts.aramalloc(size));
    }
    return ADDR_TO_PTR(void, SNDARAM_alloc(size));
}

int SNDPLATFORM_memfree(int type, unsigned int addr) {
    if (sndgs.opts.aramfree != NULL) {
        sndgs.opts.aramfree(addr);
        return 0;
    }
    SNDARAM_free(addr);
    return 0;
}

// Queues a transfer to ARAM. Returns its handle.
int SNDPLATFORM_download(int type, void* src, void* dest, int size) {
    DCFlushRange(src, size);
    int handle = snddrv.dmaId;
    postdma(-1, ARQ_PRIORITY_LOW, (u32)PTR_TO_ADDR(src), (u32)PTR_TO_ADDR(dest), size);
    return handle;
}

// 1 once the transfer is done; a transfer pending for over two seconds is queued again.
int SNDPLATFORM_downloadcomplete(int handle) {
    BOOL old = OSDisableInterrupts();
    for (int i = 0; i < 64; i++) {
        if (snddrv.dma[i].id != -1 && snddrv.dma[i].id == handle) {
            if (sndgs.serverTicks - snddrv.dma[i].time > 200) {
                OSRestoreInterrupts(old);
                BOOL old2 = OSDisableInterrupts();
                for (int j = 0; j < 64; j++) {
                    if (snddrv.dma[j].id == handle) {
                        ARQRemoveRequest(&snddrv.dma[j].req);
                        postdma(j, snddrv.dma[j].priority, snddrv.dma[j].src, snddrv.dma[j].dst, snddrv.dma[j].len);
                    }
                }
                OSRestoreInterrupts(old2);
            } else {
                OSRestoreInterrupts(old);
            }
            return 0;
        }
    }
    OSRestoreInterrupts(old);
    return 1;
}

void SNDI_mutexalloc() {
    OSInitMutex(&snddrv.mutex);
}

void SNDI_mutexfree() {
}

void SNDI_mutexlock() {
    OSLockMutex(&snddrv.mutex);
}

void SNDI_mutexunlock() {
    OSUnlockMutex(&snddrv.mutex);
}
