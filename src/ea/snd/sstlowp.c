#include "ea/snd/sst.h"

extern "C" int SNDSTRM_lowpass(int stream, int freq) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.lowpass = freq;
    SNDCTRL_lowpass(s->voice, freq);
    return 0;
}
