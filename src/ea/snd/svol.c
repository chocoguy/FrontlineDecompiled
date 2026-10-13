#include "ea/snd/sndi.h"

extern "C" int SNDvol(int handle, int vol) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        int newVol = vol << 16;
        while (iSNDpatchkey(v, &key)) {
            SNDVOICE* voice = &sndgs.voices[key];
            voice->volStep = 0;
            if (voice->vol == newVol) {
                return 0;
            }
            voice->vol = newVol;
            iSNDcalcvol(key);
            SNDPLATFORM_setvol(key);
            if (voice->fxOut[0] > 0) {
                SNDPLATFORM_setfxlevel(voice->chanVoice[0], 0);
            }
        }
    }
    return v;
}
