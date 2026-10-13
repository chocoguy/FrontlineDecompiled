#include "ea/snd/sst.h"

extern "C" int SNDSTRM_queuerequestid(int stream, int hold, int request) {
    return SNDSTRMI_queue(stream, hold, NULL, request, 2);
}
