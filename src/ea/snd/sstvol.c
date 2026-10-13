#include "ea/snd/sst.h"

extern "C" int SNDSTRM_vol(int stream, int vol) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.vol = vol;
    SNDvol(s->voice, vol);
    return 0;
}
