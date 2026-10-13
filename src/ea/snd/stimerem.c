#include "ea/snd/sndi.h"

// Centiseconds until the longest-playing part of the sound ends; -7 if unknown.
extern "C" int SNDtimeremaining(int handle) {
    int remaining = -7;
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            SNDVOICE* voice = &sndgs.voices[key];
            int frame;
            if (voice->unk_0x14 > 0) {
                frame = -7;
            } else {
                frame = SNDPLATFORM_getcurframe(key);
            }
            if (frame >= 0) {
                int t = (voice->endFrame - frame) * 100 / voice->rate;
                if (voice->pitchMult2 > 0x10) {
                    t = ((t << 12) / voice->pitchMult2 << 12) / voice->timeMult;
                } else {
                    t = 0x7FFFFFFF;
                }
                if (t > remaining) {
                    remaining = t;
                }
            }
        }
    }
    return remaining;
}
