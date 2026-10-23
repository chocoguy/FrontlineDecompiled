#include "ea/snd/sndi.h"

// Plays each timbre of a patch that covers the key and velocity. A multi-timbre sound's voices
// share a patch group (unk_0x22) so they are controlled together; the last one is its master.
int SNDBANKI_playpatch(void* bank, TAGGEDPATCH* patch, int bankIndex, int patchIndex, SNDPLAYOPTS* opts) {
    static u8 freekey = 0;
    void* data;
    int handles[12];
    SNDIPATCHHEADER header;
    SNDVOICE* voice = NULL;
    int n = 0;
    int first = 0;
    int detune = 0;
    int result = -9;
    int i;

    data = patch;
    if (patch == NULL) {
        return -8;
    }
    if (bank == NULL) {
        bank = patch;
    }
    s8 key = opts->key;
    data = (u8*)patch + 4;
    s8 vel = opts->unk_0x3;

    u8 group;
    for (;;) {
        if (++freekey == 0) {
            freekey++;
        }
        group = freekey;
        SNDVOICE* v = sndgs.voices;
        for (i = sndgs.numVoices; i > 0; i--, v++) {
            if ((s8)v->state != 0 && v->unk_0x22 == group) {
                break;
            }
        }
        if (i <= 0) {
            break;
        }
    }

    for (;;) {
        int more = SNDI_parsetimbre(&data, &header);
        if (!first) {
            if (header.unk_0x0 != 0) {
                detune = randrange(header.unk_0x0);
            }
            first = 1;
        }
        if (vel >= header.velLo && vel <= header.velHi && key >= header.keyLo && key <= header.keyHi) {
            result = SNDBANKI_playtimbre(bankIndex, patchIndex, bank, opts, &header, key, vel, detune);
            if (result < 0) {
                goto fail;
            }
            handles[n++] = result;
            SNDBANKI_userdatacallback(&header, 1);
        }
        if (!more) {
            break;
        }
    }

    {
        int count = n;
        n = 0;
        for (i = 0; i < count; i++) {
            if (!SNDover(handles[i])) {
                handles[n++] = handles[i];
            }
        }
    }
    if (n != 0) {
        if (n == 1) {
            int v = SNDVOICEI_get(handles[0]);
            if (v >= 0) {
                sndgs.voices[v].unk_0x22 = 0;
                sndgs.voices[v].unk_0x23 = 0;
            }
        } else {
            for (i = 0; i < n; i++) {
                int v = SNDVOICEI_get(handles[i]);
                if (v >= 0) {
                    voice = &sndgs.voices[v];
                    voice->unk_0x22 = group;
                    voice->unk_0x23 = 0;
                }
            }
            voice->unk_0x23 = 1;
        }
        return result;
    }
fail:
    for (i = 0; i < n; i++) {
        SNDstop(handles[i]);
    }
    return result;
}

// Starts one timbre on voices of the first render mode that has them free. Returns its handle.
int SNDBANKI_playtimbre(int bankIndex, int patchIndex, void* bank, SNDPLAYOPTS* opts, SNDIPATCHHEADER* header,
                        int key, int vel, int detune) {
    int handle = -9;
    int mode = 0;
    int lo;
    int hi;
    int renderMode;
    s8 pan = opts->unk_0x1;
    s8 fx = opts->fxlevel;
    int vol = (s8)opts->vol << 16;

    while ((renderMode = SNDI_validrendermode(&mode, header)) != 0) {
        SNDPLATFORM_getvoicerange(renderMode, &lo, &hi);
        int v = SNDVOICEI_alloc((s8)header->channels, (s8)header->unk_0xC, &handle, lo, hi);
        if (v < 0) {
            continue;
        }
        SNDVOICE* voice = &sndgs.voices[v];

        s8 p = header->pan;
        if ((s8)header->panRand != 0) {
            p += randrange((s8)header->panRand);
            if (p < 0) {
                p = 0;
            } else if (p > 0x7F) {
                p = 0x7F;
            }
        }
        voice->azimuthBase = SNDI_pantoazimuth(p);
        voice->detune = header->unk_0x4;
        voice->detune -= ((s8)header->rootKey - key) * 100;
        if (detune != 0) {
            voice->detune += detune;
        }
        if (header->unk_0x6 != 0) {
            voice->detune += randrange(header->unk_0x6);
        }
        voice->volCurve = ADDR_TO_PTR(u8, header->unk_0x20);
        voice->unk_0x68 = ADDR_TO_PTR(u8, header->unk_0x24);
        voice->volStep = 0;
        voice->vol = vol;

        s8 tv = header->vol;
        if ((s8)header->volRand != 0) {
            tv += randrange((s8)header->volRand);
        }
        if (tv > 0x7F) {
            tv = 0x7F;
        }
        if (tv < 0) {
            tv = 0;
        }
        voice->unk_0x44 = tv * vel / 127;
        voice->unk_0x76 = (s8)header->unk_0x10 * 100;
        voice->unk_0x58 = pan;

        voice->env = header->env;
        voice->envLevel = (s8)header->unk_0x1A << 16;
        voice->envSeg = 0;
        voice->envNumSegs = header->unk_0xF;
        voice->unk_0x52 = header->unk_0xE;
        voice->envTicks = voice->env[0].time;
        if (voice->envTicks < 0) {
            voice->envTicks = 0x7FFFFFFF;
        }
        voice->envStep = ((voice->env[0].level << 16) - voice->envLevel) / voice->envTicks;

        voice->volLfo = ADDR_TO_PTR(s8, header->unk_0x2C);
        voice->pitchLfo = ADDR_TO_PTR(s8, header->unk_0x30);
        voice->volLfoLen = header->unk_0x1B;
        voice->pitchLfoLen = header->unk_0x1D;
        voice->pitchLfoDepth = header->unk_0x54;
        if ((s8)header->unk_0x1C != 0) {
            voice->volLfoPos = iSNDrandom() % (s8)header->unk_0x1C;
        } else {
            voice->volLfoPos = 0;
        }
        if (header->unk_0x1E != 0) {
            voice->pitchLfoPos = iSNDrandom() % header->unk_0x1D;
        } else {
            voice->pitchLfoPos = 0;
        }

        voice->timeMult = opts->timemult;
        voice->pitchMult = opts->pitchmult;
        voice->linearPitch = 0;
        iSNDcalcpitch(v);
        voice->bank = bankIndex;
        voice->patch = patchIndex;
        voice->azimuth = voice->azimuthBase + opts->azimuth;
        voice->distance = opts->distance;
        voice->unk_0x14 = header->loopEnd;
        voice->endFrame = header->samples;
        voice->rate = header->rate;
        voice->unk_0x1E = header->unk_0x13;
        voice->channels = header->channels;
        voice->renderMode = renderMode;
        for (int i = 0; i < (s8)header->channels; i++) {
            voice->chanAzimuth[i] = header->azimuth[i];
        }
        voice->unk_0x53 = opts->drylevel;
        voice->fxSend[0] = header->unk_0x16;
        voice->fxLevel[0] = fx;
        SNDI_calcfxlevel(0, v);
        iSNDcalcvol(v);
        if (SNDPLATFORM_playtimbre(header, bank, v, opts->timemult, opts->unk_0x12, opts->lowpass, opts->highpass) >= 0) {
            break;
        }
        for (int i = 0; i < (s8)header->channels; i++) {
            SNDVOICEI_free(sndgs.voices[v].chanVoice[i]);
        }
        handle = -9;
    }
    return handle;
}
