#include "ea/snd/sst.h"

extern "C" int SNDSTRM_modifyhold(int request, int hold) {
    int result = -8;
    SNDSTREAMREQUEST* req;

    SNDSYS_entercritical();
    req = SNDSTRMI_getrequestptr(request);
    if (req != NULL) {
        result = 0;
        req->hold = (s16)hold;
    }
    SNDSYS_leavecritical();
    return result;
}
