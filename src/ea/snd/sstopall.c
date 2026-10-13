#include "ea/snd/sndi.h"

extern "C" void SNDstopall(void) {
    SNDSYS_entercritical();
    for (int i = 0; i < sndgs.numVoices; i++) {
        SNDstop(sndgs.voices[i].handle);
    }
    SNDSYS_leavecritical();
}
