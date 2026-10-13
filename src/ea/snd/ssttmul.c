#include "ea/snd/sst.h"

extern "C" int SNDSTRM_timemult(int stream, int mult) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.timemult = mult;
    SNDCTRL_timemult(s->voice, mult);
    return 0;
}
