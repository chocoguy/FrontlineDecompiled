#include "ea/snd/smix.h"
#include <string.h>

SNDMIX sndmix;

// dest[i] += level * (16 - i) / 17, then level = 0: fades out a stopped voice's last level.
void MIXI_interpolateto0(f32* level, f32* dest) {
    dest[0] += 0.9411765f * *level;
    dest[1] += 0.88235295f * *level;
    dest[2] += 0.8235294f * *level;
    dest[3] += 0.7647059f * *level;
    dest[4] += 0.7058824f * *level;
    dest[5] += 0.64705884f * *level;
    dest[6] += 0.5882353f * *level;
    dest[7] += 0.5294118f * *level;
    dest[8] += 0.47058824f * *level;
    dest[9] += 0.4117647f * *level;
    dest[10] += 0.3529412f * *level;
    dest[11] += 0.29411766f * *level;
    dest[12] += 0.23529412f * *level;
    dest[13] += 0.1764706f * *level;
    dest[14] += 0.11764706f * *level;
    dest[15] += 0.05882353f * *level;
    *level = 0.0f;
}

// Mixes 16 frames with the gain moving from `from` towards `to`.
void MIXI_interpolatemix(f32 from, f32 to, f32* src, f32* dest) {
    f32 step = 0.05882353f * (to - from);
    dest[0] += src[0] * (from + step);
    dest[1] += src[1] * (from + 2.0f * step);
    dest[2] += src[2] * (from + 3.0f * step);
    dest[3] += src[3] * (from + 4.0f * step);
    dest[4] += src[4] * (from + 5.0f * step);
    dest[5] += src[5] * (from + 6.0f * step);
    dest[6] += src[6] * (from + 7.0f * step);
    dest[7] += src[7] * (from + 8.0f * step);
    dest[8] += src[8] * (from + 9.0f * step);
    dest[9] += src[9] * (from + 10.0f * step);
    dest[10] += src[10] * (from + 11.0f * step);
    dest[11] += src[11] * (from + 12.0f * step);
    dest[12] += src[12] * (from + 13.0f * step);
    dest[13] += src[13] * (from + 14.0f * step);
    dest[14] += src[14] * (from + 15.0f * step);
    dest[15] += src[15] * (from + 16.0f * step);
}

// Mixes the voice's next 16 frames while ramping to its new gains. Returns the frames mixed.
int SNDMIXI_volramp(SNDMIXVOICE* mv) {
    int i;
    if (mv->chain->process(mv->chain, 16, sndmix.buf[0], sndmix.buf[1], 0) <= 0) {
        mv->wetCur = mv->wet[0];
        for (i = 0; i < sndmix.numOutputs; i++) {
            mv->dryCur[i] = mv->dry[i];
        }
        return 0;
    }
    mv->last = sndmix.buf[1][15];
    if (sndmix.fxMode[0]) {
        if (mv->wetCur != mv->wet[0]) {
            MIXI_interpolatemix(mv->wetCur, mv->wet[0], sndmix.buf[1], sndfx.buf);
            mv->wetCur = mv->wet[0];
        } else if (0.0f != mv->wet[0]) {
            sndmix.mix(16, sndmix.buf[1], sndfx.buf, mv->wet[0]);
        }
    }
    for (i = 0; i < sndmix.numOutputs; i++) {
        if (mv->dryCur[i] != mv->dry[i]) {
            MIXI_interpolatemix(mv->dryCur[i], mv->dry[i], sndmix.buf[1], sndmix.out[i]);
            mv->dryCur[i] = mv->dry[i];
        } else if (0.0f != mv->dry[i]) {
            sndmix.mix(16, sndmix.buf[1], sndmix.out[i], mv->dry[i]);
        }
    }
    return 16;
}

// Allocator hooks for the codecs (coda.cpp).
void* SNDI_New(unsigned long size) {
    return SNDMEMI_allocz(size);
}

void SNDI_Delete(void* ptr) {
    SNDMEMI_free(ptr);
}

extern "C" void MIX_create(MIXOPTS* opts) {
    int i;

    SND::CODASetNew(SNDI_New);
    SND::CODASetDelete(SNDI_Delete);
    sndmix.fxMode[0] = 0;
    sndmix.rate = opts->rate;
    sndmix.numVoices = opts->numVoices;
    sndmix.numOutputs = opts->numOutputs;
    sndmix.unk_0x6 = opts->unk_0x6;
    sndmix.unk_0x7 = opts->unk_0x7;
    sndmix.stopcb = opts->stopcb;
    SNDSYS_entercritical();
    for (i = 0; i < 2; i++) {
        sndmix.bufMem[i] = SNDMEMI_allocz(0x14BC);
        sndmix.buf[i] = (f32*)sndmix.bufMem[i];
        sndmix.buf[i] += 2;
        f32* p = sndmix.buf[i];
        while ((uptr)(p) & 0x3F) {
            p++;
        }
        sndmix.buf[i] = p;
    }
    for (i = 0; i < sndmix.numOutputs; i++) {
        sndmix.outMem[i] = SNDMEMI_allocz(0x2C0);
        sndmix.out[i] = (f32*)sndmix.outMem[i];
        f32* p = sndmix.out[i];
        while ((uptr)(p) & 0x3F) {
            p++;
        }
        sndmix.out[i] = p;
    }
    if (sndmix.numVoices != 0) {
        sndmix.voices = (SNDMIXVOICE*)SNDMEMI_allocz(sndmix.numVoices * sizeof(SNDMIXVOICE));
        memset(sndmix.voices, 0, sndmix.numVoices * sizeof(SNDMIXVOICE));
    }
    SNDSYS_leavecritical();
    MIXI_initunpack16();
    MIXI_initunpackxa();
    MIXI_initunpackmt();
    MIX_reset();
    for (i = 0; i < sndmix.numOutputs; i++) {
        sndmix.outChain[i] = NULL;
        SFILTER_ft24_32init(&sndmix.ft[i]);
        sndmix.ft[i].f.order = 0;
        SFILTER_add(&sndmix.outChain[i], &sndmix.ft[i].f);
        memset(sndmix.out[i], 0, 0x280);
    }
}

extern "C" void MIX_destroy(void) {
    int i;

    SNDSYS_entercritical();
    if (sndmix.voices != NULL) {
        SNDMEMI_free(sndmix.voices);
        sndmix.voices = NULL;
    }
    for (i = 0; i < 2; i++) {
        if (sndmix.bufMem[i] != NULL) {
            SNDMEMI_free(sndmix.bufMem[i]);
            sndmix.bufMem[i] = NULL;
        }
    }
    for (i = 0; i < sndmix.numOutputs; i++) {
        if (sndmix.outMem[i] != NULL) {
            SNDMEMI_free(sndmix.outMem[i]);
            sndmix.outMem[i] = NULL;
        }
    }
    SNDSYS_leavecritical();
}

extern "C" void MIX_reset(void) {
    sndmix.mix = mixc;
    sndmix.decode = decode16x87;
    sndmix.frames = 320;
}

// Sets up a voice's decoder (chosen by sample format and mode) and optional time stretch.
extern "C" void MIX_playinit(int voice, int format, int mode, int a, int b, u8* stretch, int c, int d, int e, int f,
                             int g, int h, int i) {
    UNPACKINITPARAMS init;
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    int index;

    mv->chain = NULL;
    mv->decoder = NULL;
    mv->rsf = NULL;
    mv->lpf = NULL;
    mv->hpf = NULL;
    mv->stretch = NULL;
    index = 0;
    init.decode = 1;
    init.getframe = NULL;
    switch (format) {
    case 8:
        index = 3;
        break;
    case 7:
        index = 3;
        break;
    case 9:
        init.decode = 0;
        index = 0;
        break;
    case 10:
        index = 6;
        break;
    case 4:
        index = 9;
        break;
    case 14:
        index = 12;
        break;
    case 15:
        index = 15;
        break;
    case 16:
        index = 18;
        break;
    case 0x40:
        index = 21;
        break;
    }
    if (mode == 1) {
        if (f > 0) {
            index += 1;
        }
    } else if (mode == 0) {
        index += 2;
    }
    if (sndmix.decoderInit[index] != NULL) {
        mv->decoder = (SFILTER*)SNDMEMI_allocz(sndmix.decoderSize[index]);
        init.data = ADDR_TO_PTR(void, a);
        init.unk_0x4 = b;
        init.unk_0x8 = c;
        init.samples = d;
        init.loopStart = e;
        init.loopEnd = f;
        init.voice = voice;
        init.quality = sndgs.unk_0x16F;
        mv->decoder->destroy = NULL;
        mv->decoder->order = 0xF0;
        mv->decoder->port = i;
        sndmix.decoderInit[index](mv->decoder, &init);
        mv->getframe = init.getframe;
        SFILTER_add(&mv->chain, mv->decoder);
    }
    if (stretch != NULL) {
        if (mode != 0) {
            voice = -1;
        }
        mv->stretch = (TIMESTRETCHSTATE*)SNDMEMI_allocz(GC_SIZE(0x1828, sizeof(TIMESTRETCHSTATE)));
        ((SFILTER*)mv->stretch)->destroy = NULL;
        ((SFILTER*)mv->stretch)->order = 0xC8;
        SFILTER_timestretchinit(mv->stretch, stretch, voice);
        SFILTER_add(&mv->chain, (SFILTER*)mv->stretch);
    }
    mv->state = 1;
}

extern "C" void MIX_play(int voice) {
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    mv->last = 0.0f;
    mv->rampPending = 0;
    mv->wetCur = mv->wet[0];
    for (int i = 0; i < sndmix.numOutputs; i++) {
        mv->dryCur[i] = mv->dry[i];
    }
    mv->state = 2;
}

// Frees the voice's chain; its last level is faded out by the next slice.
extern "C" void MIX_stop(int voice) {
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    SFILTER* next;
    sndmix.fxTail += mv->wetCur * mv->last;
    for (int i = 0; i < sndmix.numOutputs; i++) {
        sndmix.outTail[i] += mv->dryCur[i] * mv->last;
    }
    do {
        if (mv->chain->destroy != NULL) {
            mv->chain->destroy(mv->chain);
        }
        next = mv->chain->next;
        SNDMEMI_free(mv->chain);
        mv->chain = next;
    } while (next != NULL);
    mv->state = 0;
}

extern "C" void SNDMIX_setdrygain(int voice, int output, f32 gain) {
    sndmix.voices[voice].dry[output] = gain;
    sndmix.voices[voice].rampPending = 1;
}

extern "C" void SNDMIX_setwetgain(int voice, int bus, f32 gain) {
    if ((u8)sndmix.fxMode[bus] == 2) {
        sndmix.voices[voice].wet[bus] = 1.5f * gain;
    } else {
        sndmix.voices[voice].wet[bus] = gain;
    }
    sndmix.voices[voice].rampPending = 1;
}

// Mixes all playing voices and runs each output's chain into dest[output].
extern "C" void MIX_audioslice(f32** dest, int frames) {
    int i;
    int v;

    if (sndmix.fxMode[0] && sndmix.fxTail != 0.0f) {
        MIXI_interpolateto0(&sndmix.fxTail, sndfx.buf);
    }
    for (i = 0; i < sndmix.numOutputs; i++) {
        memset(sndmix.out[i], 0, frames * 4);
        if (sndmix.outTail[i] != 0.0f) {
            MIXI_interpolateto0(&sndmix.outTail[i], sndmix.out[i]);
        }
    }
    for (v = 0; v < sndmix.numVoices; v++) {
        SNDMIXVOICE* mv = &sndmix.voices[v];
        if (mv->state != 2) {
            continue;
        }
        int start;
        int left;
        if (mv->rampPending) {
            start = SNDMIXI_volramp(mv);
            mv->rampPending = 0;
            left = frames - start;
        } else {
            start = 0;
            left = frames;
        }
        if (left == 0) {
            continue;
        }
        int got = mv->chain->process(mv->chain, left, sndmix.buf[0], sndmix.buf[1], 0);
        if (got < 0) {
            MIX_stop(v);
            sndmix.stopcb(v);
        } else if (got != 0) {
            mv->last = sndmix.buf[1][got - 1];
            for (i = 0; i < sndmix.numOutputs; i++) {
                if (mv->dry[i] != 0.0f) {
                    sndmix.mix(got, sndmix.buf[1], sndmix.out[i] + start, mv->dry[i]);
                }
            }
            if (sndmix.fxMode[0] && mv->wet[0] != 0.0f) {
                sndmix.mix(got, sndmix.buf[1], sndfx.buf + start, mv->wet[0]);
                sndfx.silent = 0;
            }
        }
    }
    if (sndmix.postmix != NULL) {
        sndmix.postmix(frames);
    }
    for (i = 0; i < sndmix.numOutputs; i++) {
        if (sndmix.outPre[i] != NULL) {
            sndmix.outPre[i]->process(sndmix.outPre[i], frames, sndmix.out[i], sndmix.buf[0], 0);
            sndmix.outChain[i]->process(sndmix.outChain[i], frames, sndmix.buf[0], dest[i], 0);
        } else {
            sndmix.outChain[i]->process(sndmix.outChain[i], frames, sndmix.out[i], dest[i], 0);
        }
    }
}
