#include "ea/snd/sst.h"

extern "C" int SNDSTRM_queuemem(int stream, int hold, char* mem, int offset) {
    return SNDSTRMI_queue(stream, hold, mem + offset, 0, 1);
}
