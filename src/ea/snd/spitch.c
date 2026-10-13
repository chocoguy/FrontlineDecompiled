#include "ea/snd/sndi.h"

extern "C" int SNDpitchmult(int handle, int mult) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            SNDVOICE* voice = &sndgs.voices[key];
            if (voice->pitchMult == mult) {
                return 0;
            }
            voice->pitchMult = mult;
            iSNDcalcpitch(key);
            SNDPLATFORM_setpitch(key);
        }
    }
    return v;
}
