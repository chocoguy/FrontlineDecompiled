#include "ea/snd/sndi.h"

extern "C" int SNDCTRL_drylevel(int handle, int level) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            sndgs.voices[key].unk_0x53 = level;
            SNDPLATFORM_setdrylevel(key);
        }
    }
    return v;
}
