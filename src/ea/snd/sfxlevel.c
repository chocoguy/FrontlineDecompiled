#include "ea/snd/sndi.h"

extern "C" int SNDfxlevel(int handle, int bus, int level) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            sndgs.voices[key].fxLevel[bus] = level;
            SNDI_calcfxlevel(bus, key);
            SNDPLATFORM_setfxlevel(key, bus);
        }
    }
    return v;
}
