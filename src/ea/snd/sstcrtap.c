#include "ea/snd/sst.h"

extern "C" int SNDSTRM_createtap(int tap, SNDPLAYOPTS* opts, int numRequests, int frames, void* buffer, int size) {
    return SNDSTRMI_create(opts, numRequests, frames, buffer, size, tap, 1);
}
