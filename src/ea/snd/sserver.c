#include "ea/snd/sndi.h"

// 100 Hz tick: serve the platform, run the clients, then step each playing voice's LFOs,
// autovol ramp and envelope.
void SNDSYSI_100hzserver() {
    int i;
    int offset;

    sndgs.serverTicks++;
    iSNDserve();
    for (i = 0; i < sndgs.num100hzClients; i++) {
        sndgs.client100hz[i]();
    }

    for (i = 0, offset = 0; i < sndgs.numVoices; i++, offset += 0x80) {
        SNDVOICE* v = (SNDVOICE*)((u8*)sndgs.voices + offset);
        int changed;

        if (v->state != 1 || v->handle < 0) {
            continue;
        }
        if (v->pitchLfo != NULL) {
            v->pitchLfoPos++;
            if (v->pitchLfoPos >= v->pitchLfoLen) {
                v->pitchLfoPos = 0;
            }
            v->linearPitch = 0;
            iSNDcalcpitch(i);
            SNDPLATFORM_setpitch(i);
        }
        changed = 0;
        if (v->volLfo != NULL) {
            changed = 1;
            v->volLfoPos++;
            if (v->volLfoPos >= v->volLfoLen) {
                v->volLfoPos = 0;
            }
        }
        if (v->volStep != 0) {
            changed = 1;
            v->vol += v->volStep;
            if (v->volStep < 0) {
                if (v->vol <= v->volTarget) {
                    v->vol = v->volTarget;
                    v->volStep = 0;
                }
            } else {
                if (v->vol >= v->volTarget) {
                    v->vol = v->volTarget;
                    v->volStep = 0;
                }
            }
            if (v->vol < 0) {
                SNDstop(v->handle);
                continue;
            }
        }
        v->envTicks--;
        if (v->envStep != 0) {
            changed = 1;
            v->envLevel += v->envStep;
        }
        if (v->envTicks == 0) {
            SNDENVSEG* seg;
            v->envSeg++;
            if (v->envSeg >= v->envNumSegs) {
                SNDstop(v->handle);
                continue;
            }
            seg = &v->env[v->envSeg];
            v->envTicks = seg->time;
            if (v->envTicks < 0) {
                v->envTicks = 0x7FFFFFFF;
            }
            v->envStep = ((seg->level << 16) - v->envLevel) / v->envTicks;
        }
        if (changed) {
            iSNDcalcvol(i);
            if (v->handle >= 0) {
                SNDPLATFORM_setvol(i);
            }
        }
    }
}

extern "C" void SNDSYS_entercritical(void) {
    SNDI_mutexlock();
    sndgs.critical++;
}

extern "C" void SNDSYS_leavecritical(void) {
    sndgs.critical--;
    SNDI_mutexunlock();
}
