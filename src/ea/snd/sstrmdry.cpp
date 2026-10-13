#include "ea/snd/sst.h"

extern "C" int SNDSTRM_drylevel(int stream, int level) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.drylevel = level;
    SNDCTRL_drylevel(s->voice, level);
    return 0;
}
