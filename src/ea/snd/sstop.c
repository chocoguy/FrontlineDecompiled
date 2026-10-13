#include "ea/snd/sndi.h"

extern "C" int SNDstop(int handle) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            SNDPLATFORM_stop(key);
        }
    }
    return v;
}
