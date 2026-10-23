#include "ea/snd/aems.h"

static inline int AEMS_clampu16(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 0xFFFF) {
        return 0xFFFF;
    }
    return v;
}

static inline int AEMS_clamp7fff(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 0x7FFF) {
        return 0x7FFF;
    }
    return v;
}

static inline int AEMS_clampdist(int v) {
    if (v < -0x4000) {
        return -0x4000;
    }
    if (v > 0x3FFF) {
        return 0x3FFF;
    }
    return v;
}

// Player for bank patches: start on the gate's rising edge, stop on 0 (2 holds), and while
// playing follow the inputs and report time left and position.
void SNDAEMSI_updateplayerbank(AEMSCOMPDYNAMICPLAYER* d) {
    AEMSCOMP* comp = d->comp;
    MODULEBANK* bank;

    if (d->lastGate != d->in[7]) {
        int gate = d->in[7];
        if (gate == 1) {
            if (d->lastGate == 0) {
                AEMSPLAYERTABLE* table = ADDR_TO_PTR(AEMSPLAYERTABLE, comp->p[0]);
                int index = d->in[6];
                SNDPLAYOPTS opts;
                if (index >= table->count) {
                    index = table->count - 1;
                }
                d->entry = table->entries[index];
                SNDplaysetdef(&opts);
                d->pitch = AEMS_clampu16(d->in[0]);
                opts.pitchmult = d->pitch;
                d->vol = AEMS_clamp7fff(d->in[2]);
                opts.vol = d->vol >> 8;
                d->azimuth = d->in[3];
                opts.azimuth = d->azimuth;
                d->distance = AEMS_clampdist(d->in[4]);
                opts.distance = d->distance;
                d->fx = AEMS_clamp7fff(d->in[5]);
                opts.fxlevel = d->fx >> 8;
                d->lowpass = AEMS_clampu16(d->in[8]);
                opts.lowpass = d->lowpass;
                d->highpass = AEMS_clampu16(d->in[9]);
                opts.highpass = d->highpass;
                d->timemult = AEMS_clampu16(d->in[1]);
                opts.timemult = d->timemult;
                if ((ADDR_TO_PTR(MODULEBANK, comp->p[1]))->version >= 9) {
                    d->dry = AEMS_clamp7fff(d->in[10]);
                    opts.drylevel = d->dry >> 8;
                }
                bank = ADDR_TO_PTR(MODULEBANK, comp->p[1]);
                d->handle = SNDBANK_play(bank->rt->sfxBank, d->entry & 0x3FFF, &opts);
                if (d->handle >= 0) {
                    d->out = 1;
                    d->timeLeft = 0x7FFFFFFF;
                    d->position = 0;
                } else {
                    d->out = 0;
                    d->timeLeft = 0;
                    d->position = 0;
                }
            }
        } else if (gate == 2) {
            if (d->lastGate != 1) {
                d->in[7] = 0;
            }
        } else if (gate == 0) {
            SNDstop(d->handle);
            d->handle = -1;
            d->out = 0;
            d->timeLeft = 0;
            d->position = 0;
        }
        d->lastGate = d->in[7];
    }

    if (d->lastGate == 1) {
        int v = SNDVOICEI_get(d->handle);
        SNDVOICE* voice;
        if (v < 0) {
            d->out = 0;
            d->timeLeft = 0;
            d->position = 0;
            return;
        }
        voice = &sndgs.voices[v];
        if (comp->outputs[1].count > 0) {
            d->timeLeft = SNDtimeremaining(d->handle) * 10;
            if (d->timeLeft < 0) {
                d->timeLeft = 0x7FFFFFFF;
            }
        }
        if (comp->outputs[2].count > 0) {
            d->position = SNDPLATFORM_getcurframe(v);
            if (d->position > 2000000) {
                d->position = iSNDdivu64(iSNDmulu64(d->position, 1000), voice->rate);
            } else if (d->position >= 0) {
                d->position = d->position * 1000 / voice->rate;
            } else {
                d->position = 0;
            }
        }
        if (d->in[0] != d->pitch) {
            d->pitch = AEMS_clampu16(d->in[0]);
            voice->pitchMult2 = d->pitch;
            SNDPLATFORM_setpitch(v);
        }
        if (d->in[2] != d->vol) {
            d->vol = AEMS_clamp7fff(d->in[2]);
            voice->vol = d->vol << 8;
            voice->unk_0x45 = (d->vol * (s8)sndgs.masterVol) / 0x7FFF;
            SNDPLATFORM_setvol(v);
        }
        if (d->in[3] != d->azimuth || d->in[4] != d->distance) {
            d->azimuth = d->in[3];
            d->distance = AEMS_clampdist(d->in[4]);
            voice->azimuth = d->azimuth;
            voice->distance = d->distance;
            SNDPLATFORM_set3dpos(v);
        }
        if (d->in[5] != d->fx) {
            d->fx = AEMS_clamp7fff(d->in[5]);
            voice->fxLevel[0] = d->fx >> 8;
            SNDI_calcfxlevel(0, v);
            SNDPLATFORM_setfxlevel(v, 0);
        }
        if (d->in[8] != d->lowpass) {
            d->lowpass = AEMS_clampu16(d->in[8]);
            SNDPLATFORM_lowpass(v, d->lowpass);
        }
        if (d->in[9] != d->highpass) {
            d->highpass = AEMS_clampu16(d->in[9]);
            SNDPLATFORM_highpass(v, d->highpass);
        }
        if (d->in[1] != d->timemult) {
            d->timemult = AEMS_clampu16(d->in[1]);
            voice->timeMult = d->timemult;
            SNDPLATFORM_timemult(v, d->timemult);
        }
        if ((ADDR_TO_PTR(MODULEBANK, comp->p[1]))->version >= 9 && d->in[10] != d->dry) {
            d->dry = AEMS_clamp7fff(d->in[10]);
            SNDCTRL_drylevel(d->handle, (s8)(d->dry >> 8));
        }
    }
}

void SNDAEMS_linkbank() {
    sndaems.updatePlayerBank = SNDAEMSI_updateplayerbank;
}
