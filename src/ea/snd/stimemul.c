#include "ea/snd/sndi.h"

extern "C" int SNDCTRL_timemult(int handle, int mult) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            sndgs.voices[key].timeMult = mult;
            SNDPLATFORM_timemult(key, mult);
        }
    }
    return 0;
}
