#include "ea/snd/sndi.h"

// 0..127 output volume from the master, envelope, play and current volumes.
void iSNDcalcvol(int voice) {
    SNDVOICE* v = &sndgs.voices[voice];
    v->unk_0x45 = (s8)sndgs.masterVol * ((v->envLevel >> 16) * (v->unk_0x44 * (v->vol >> 16))) / (127 * 127 * 127);
    if (v->volLfo != NULL) {
        v->unk_0x45 = v->unk_0x45 * v->volLfo[v->volLfoPos] / 127;
    }
    if (v->volCurve != NULL) {
        v->unk_0x45 = v->volCurve[v->unk_0x45];
    }
}
