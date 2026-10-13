#include "ea/snd/sndi.h"

// Steps *key through the voices playing the same patch as `voice` (start with -1).
int iSNDpatchkey(int voice, int* key) {
    u8 patch = sndgs.voices[voice].unk_0x22;
    if (patch != 0) {
        for (;;) {
            ++*key;
            if (*key >= sndgs.numVoices) {
                return 0;
            }
            SNDVOICE* v = &sndgs.voices[*key];
            if (v->unk_0x22 == patch && v->state == 1 && v->handle >= 0) {
                return 1;
            }
        }
    }
    if (*key < 0) {
        *key = voice;
        return 1;
    }
    return 0;
}
