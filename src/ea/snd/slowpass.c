#include "ea/snd/sndi.h"

extern "C" int SNDCTRL_lowpass(int handle, int freq) {
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            SNDPLATFORM_lowpass(key, freq);
        }
    }
    return v;
}
