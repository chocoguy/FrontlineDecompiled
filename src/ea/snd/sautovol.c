#include "ea/snd/sndi.h"

// Ramp the sound's voices to `vol` over `ticks` (100 Hz).
extern "C" int SNDautovol(int handle, int ticks, int vol) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        int target;
        if (ticks <= 0) {
            ticks = 1;
        }
        target = vol << 16;
        while (iSNDpatchkey(v, &key)) {
            SNDVOICE* voice = &sndgs.voices[key];
            voice->volTarget = target;
            voice->volStep = (voice->volTarget - voice->vol) / ticks;
        }
    }
    return v;
}
