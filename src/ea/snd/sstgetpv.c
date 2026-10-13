#include "ea/snd/sst.h"

// The voice's programmed volume, or the stream's when no voice is playing.
extern "C" int SNDSTRM_getprogvol(int stream) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    int vol;
    if (s == NULL) {
        return -8;
    }
    vol = SNDCTRL_getprogvol(s->voice);
    if (vol < 0) {
        vol = (s8)s->opts.vol;
    }
    return vol;
}
