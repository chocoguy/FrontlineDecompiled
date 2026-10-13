#include "ea/snd/sndi.h"

extern "C" int SNDCTRL_getprogvol(int handle) {
    int v = SNDVOICEI_get(handle);
    if (v < 0) {
        return -8;
    }
    return sndgs.voices[v].vol >> 16;
}
