#include "ea/snd/sst.h"

extern "C" int SNDSTRM_3dpos(int stream, int azimuth, int distance) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    s->opts.azimuth = azimuth;
    s->opts.distance = distance;
    SND3dpos(s->voice, azimuth, distance);
    return 0;
}
