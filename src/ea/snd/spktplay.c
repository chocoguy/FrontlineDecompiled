#include "ea/snd/spktplay.h"

SNDPKTPLAYSTATE sndpps;

extern "C" int SNDPKTPLAY_overhead(int frames) {
    return SNDPLATFORM_packetoverhead() + frames * sizeof(SNDPKTFRAME) + GC_SIZE(0x64, sizeof(SNDPKTPLAY));
}

extern "C" int SNDPKTPLAY_create(SNDPKTFRAMECB frameDone, SNDPKTFREECB framesFree, void* param, void* buffer, int size) {
    SNDPKTPLAY* p;
    int i;

    SNDSYS_entercritical();
    for (i = 0; i < sndgs.opts.caps.maxPacketPlayers; i++) {
        if (sndpps.players[i] == NULL) {
            break;
        }
    }
    if (i >= sndgs.opts.caps.maxPacketPlayers) {
        SNDSYS_leavecritical();
        return -9;
    }
    if (SNDPLATFORM_packetplaycreate(i, buffer) < 0) {
        SNDSYS_leavecritical();
        return -6;
    }
    p = (SNDPKTPLAY*)((u8*)buffer + SNDPLATFORM_packetoverhead());
    size -= SNDPLATFORM_packetoverhead();
    p->numFrames = (u32)(size - GC_SIZE(0x64, sizeof(SNDPKTPLAY))) / sizeof(SNDPKTFRAME);
    p->buffer = buffer;
    p->frameDone = frameDone;
    p->framesFree = framesFree;
    p->param = param;
    p->voice = -1;
    sndpps.players[i] = p;
    SNDSYS_leavecritical();
    return i;
}

// Allocate voices for the first render mode that works and start the platform player.
extern "C" int SNDPKTPLAY_start(int player, SNDSAMPLEFORMAT* format, SNDSAMPLEATTR* attr, SNDPLAYOPTS* opts) {
    SNDIPATCHHEADER header;
    int mode = 0;
    int channels = format->channels;
    SNDPKTPLAY* p = sndpps.players[player];
    int i;

    header.channels = format->channels;
    header.renderMode = attr->renderMode;
    p->rate = format->rate;
    p->channels = format->channels;
    p->unk_0x3B = format->unk_0x3;
    p->frameCounter = 0;
    p->samplesQueued = 0;
    p->samplesPlaying = 0;
    p->writePos = 0;
    p->notifyPos = 0;
    p->waiting = 1;
    for (i = 0; i < p->channels; i++) {
        p->queued[i] = 0;
        p->readPos[i] = 0;
        p->chanData[i] = attr->data[i];
    }

    for (;;) {
        int renderMode = SNDI_validrendermode(&mode, &header);
        int lo;
        int hi;
        int handle;
        int v;
        int result;
        int best;
        int bestChan;
        SNDVOICE* voice;

        if (renderMode == 0) {
            break;
        }
        SNDPLATFORM_getvoicerange(renderMode, &lo, &hi);
        v = SNDVOICEI_alloc(channels, 0x65, &handle, lo, hi);
        if (v < 0) {
            p->voice = -9;
            continue;
        }
        p->voice = handle;
        voice = &sndgs.voices[v];
        best = -1;
        bestChan = 0;
        for (i = 0; i < format->channels; i++) {
            if (voice->chanVoice[i] > best) {
                best = voice->chanVoice[i];
                bestChan = i;
            }
        }
        p->master = bestChan;

        voice = &sndgs.voices[v];
        voice->azimuthBase = 0;
        voice->azimuth = opts->azimuth;
        voice->bank = -1;
        voice->detune = attr->unk_0x0;
        voice->pitchMult = opts->pitchmult;
        voice->volStep = 0;
        voice->vol = (s8)opts->vol << 16;
        voice->envStep = 0;
        voice->envTicks = 0x7FFFFFFF;
        voice->envLevel = 0x7F0000;
        voice->unk_0x44 = attr->unk_0x3;
        voice->unk_0x58 = opts->unk_0x1;
        voice->envNumSegs = 1;
        voice->envSeg = 0;
        voice->unk_0x52 = 0;
        voice->fxSend[0] = attr->unk_0x5;
        voice->unk_0x53 = opts->drylevel;
        voice->fxLevel[0] = opts->fxlevel;
        voice->unk_0x76 = attr->unk_0x6 * 100;
        voice->unk_0x22 = 0;
        voice->unk_0x23 = 0;
        voice->env = NULL;
        voice->volCurve = NULL;
        voice->unk_0x68 = NULL;
        voice->volLfo = NULL;
        voice->pitchLfo = NULL;
        for (i = 0; i < format->channels; i++) {
            voice->chanAzimuth[i] = attr->unk_0xC[i];
        }
        voice->unk_0x14 = -1;
        voice->endFrame = 0;
        voice->rate = format->rate;
        voice->unk_0x1E = format->unk_0x3;
        voice->channels = format->channels;
        voice->renderMode = renderMode;
        voice->linearPitch = 0;
        iSNDcalcpitch(v);
        iSNDcalcvol(v);
        SNDI_calcfxlevel(0, v);
        result = SNDPLATFORM_packetplay(player, v, opts->timemult, opts->unk_0x12, opts->lowpass,
                                        opts->highpass, format, attr->data);
        if (result >= 0) {
            break;
        }
        for (i = 0; i < channels; i++) {
            SNDVOICEI_free(sndgs.voices[v].chanVoice[i]);
        }
        p->voice = result;
    }
    return p->voice;
}

// Queue a frame; returns its number (passed to the frame-done callback).
extern "C" u32 SNDPKTPLAY_submit(int player, SNDPKTSUBMIT* frame) {
    SNDPKTPLAY* p = sndpps.players[player];
    SNDPKTFRAME* f;
    u32 counter;
    int i;

    if (p->queued[0] >= p->numFrames - 1) {
        return (u32)-13;
    }
    f = &p->frames[p->writePos];
    f->samples = frame->samples;
    f->flag = frame->flag;
    f->counter = p->frameCounter;
    for (i = 0; i < p->channels; i++) {
        f->data[i] = frame->data[i];
        p->queued[i]++;
    }
    p->samplesQueued += frame->samples;
    counter = p->frameCounter++;
    p->writePos++;
    p->waiting = 1;
    if (p->writePos >= p->numFrames) {
        p->writePos = 0;
    }
    return counter;
}

extern "C" int SNDPKTPLAY_submitspace(int player) {
    SNDPKTPLAY* p = sndpps.players[player];
    return p->numFrames - p->queued[0] - 1;
}

extern "C" int SNDPKTPLAY_framesoutstanding(int player) {
    SNDPKTPLAY* p = sndpps.players[player];
    return p->samplesQueued + p->samplesPlaying;
}

extern "C" int SNDPKTPLAY_stop(int player) {
    SNDPKTPLAY* p = sndpps.players[player];
    int i;

    SNDstop(p->voice);
    SNDPKTPLAYI_flushcallbackdata();
    p->voice = -1;
    for (i = 0; i < p->channels; i++) {
        if (p->chanData[i] != NULL) {
            SNDMEMI_free(p->chanData[i]);
        }
    }
    return 0;
}

extern "C" int SNDPKTPLAY_destroy(int player) {
    SNDPLATFORM_packetplaydestroy(player);
    sndpps.players[player] = NULL;
    return 0;
}

// Queue a frame-done callback (with the frame's first data pointer) for the frame at
// notifyPos and move on.
static inline void SNDPKTPLAYI_notify(int player, SNDPKTPLAY* p) {
    SNDPKTFRAME* f = &p->frames[p->notifyPos];
    if (p->frameDone != NULL) {
        SNDPKTCALLBACK* cb = &sndpps.callbacks[sndpps.numCallbacks];
        cb->type = 1;
        cb->player = player;
        cb->value = (u32)PTR_TO_ADDR(f->data[0]);
        sndpps.numCallbacks++;
    }
    p->notifyPos++;
    if (p->notifyPos >= p->numFrames) {
        p->notifyPos = 0;
    }
}

// Next frame's data for a channel (platform side); -1 if it has none for the channel.
int SNDPKTPLAYI_get(int player, int chan, int* samples, int* flag) {
    SNDPKTPLAY* p = sndpps.players[player];
    SNDPKTFRAME* f;

    if (p->queued[chan] == 0) {
        if (chan == p->master && !p->waiting && p->readPos[chan] != p->notifyPos) {
            SNDPKTPLAYI_notify(player, p);
        }
        return 0;
    }
    f = &p->frames[p->readPos[chan]];
    *samples = f->samples;
    *flag = f->flag;
    if (chan == p->master) {
        if (p->readPos[chan] != p->notifyPos) {
            SNDPKTPLAYI_notify(player, p);
        }
        p->samplesQueued -= f->samples;
        p->samplesPlaying += f->samples;
    }
    p->readPos[chan]++;
    if (p->readPos[chan] >= p->numFrames) {
        p->readPos[chan] = 0;
    }
    p->queued[chan]--;
    if (p->queued[chan] == 0 && chan == p->master) {
        p->waiting = 0;
    }
    if (f->data[chan] != NULL) {
        return (int)PTR_TO_ADDR(f->data[chan]);
    }
    return -1;
}

void SNDPKTPLAYI_freeframes(int player, int chan, int samples) {
    SNDPKTPLAY* p = sndpps.players[player];
    if (chan == p->master) {
        p->samplesPlaying -= samples;
        if (p->framesFree != NULL) {
            SNDPKTCALLBACK* cb = &sndpps.callbacks[sndpps.numCallbacks];
            cb->type = 0;
            cb->player = player;
            cb->value = samples;
            sndpps.numCallbacks++;
        }
    }
}

void SNDPKTPLAYI_flushcallbackdata() {
    int i;
    for (i = 0; i < sndpps.numCallbacks; i++) {
        SNDPKTCALLBACK* cb = &sndpps.callbacks[i];
        SNDPKTPLAY* p = sndpps.players[cb->player];
        if (cb->type == 0) {
            p->framesFree(cb->player, cb->value, p->param);
        } else {
            p->frameDone(ADDR_TO_PTR(void, cb->value), p->param);
        }
    }
    sndpps.numCallbacks = 0;
}
