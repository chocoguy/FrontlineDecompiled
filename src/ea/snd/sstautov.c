#include "ea/snd/sst.h"

// `ms` is converted to the 100 Hz ticks SNDautovol takes.
extern "C" int SNDSTRM_autovol(int stream, int ms, int vol) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.vol = vol;
    SNDautovol(s->voice, ms / 10, vol);
    return 0;
}
