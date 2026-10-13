#include "ea/snd/sst.h"

extern "C" int SNDSTRM_highpass(int stream, int freq) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.highpass = freq;
    SNDCTRL_highpass(s->voice, freq);
    return 0;
}
