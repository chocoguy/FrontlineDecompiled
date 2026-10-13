#include "ea/snd/sndi.h"

extern "C" int SNDPROFILE_outputlatency(void) {
    if (sndgs.inited) {
        return SNDPLATFORM_outputlatency();
    }
    return 0;
}
