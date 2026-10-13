#include "ea/snd/sst.h"

extern "C" int SNDSTRM_fxlevel(int stream, int bus, int level) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.fxlevel = level;
    SNDfxlevel(s->voice, bus, level);
    return 0;
}
