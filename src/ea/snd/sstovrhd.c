#include "ea/snd/sst.h"

extern "C" int SNDSTRM_overheadtap(int numRequests, int frames) {
    return numRequests * sizeof(SNDSTREAMREQUEST) + sizeof(SNDSTREAMCHANNEL) + SNDPKTPLAY_overhead(frames);
}

extern "C" int SNDSTRM_overhead(int numRequests, int frames) {
    int size = numRequests * sizeof(SNDSTREAMREQUEST) + sizeof(SNDSTREAMCHANNEL) + SNDPKTPLAY_overhead(frames);
    return size + STREAM_overhead(numRequests + 2, 1, 1);
}
