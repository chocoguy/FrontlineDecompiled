#include "ea/snd/sndi.h"

extern "C" int SNDCTRL_filteradd(int handle, SNDFILTERDEF* filter) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            SNDPLATFORM_filteradd(key, filter);
        }
    }
    return v;
}
