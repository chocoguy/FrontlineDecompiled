#include "ea/snd/sst.h"

extern "C" int SNDSTRM_pitchmult(int stream, int mult) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.pitchmult = mult;
    SNDpitchmult(s->voice, mult);
    return 0;
}
