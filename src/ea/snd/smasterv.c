#include "ea/snd/sndi.h"

extern "C" int SNDmastervol(int vol) {
    int i;
    SNDSYS_entercritical();
    sndgs.masterVol = vol;
    for (i = 0; i < sndgs.numVoices; i++) {
        SNDVOICE* v = &sndgs.voices[i];
        if (v->state == 1 && v->handle >= 0) {
            iSNDcalcvol(i);
            SNDPLATFORM_setvol(i);
        }
    }
    SNDSYS_leavecritical();
    return 0;
}
