#include "ea/snd/sst.h"

extern "C" int SNDSTRM_setgreedylevel(int stream, int level) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    STREAM_setgreedylevel(s->stream, level);
    return 0;
}
