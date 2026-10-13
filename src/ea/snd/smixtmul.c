#include "ea/snd/smix.h"

extern "C" void MIX_settimemult(int voice, int mult) {
    TIMESTRETCHSTATE* ts = sndmix.voices[voice].stretch;
    if (ts != NULL) {
        SFILTER_timestretchsetratio(ts, mult);
    }
}
