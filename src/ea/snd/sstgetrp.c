#include "ea/snd/sst.h"

SNDSTREAMREQUEST* SNDSTRMI_getrequestptr(int request) {
    SNDSTREAMCHANNEL* s;
    SNDSTREAMREQUEST* req;

    if (request < 0) {
        return NULL;
    }
    s = SNDSTRMI_getstreamptr(request & 0xFF);
    if (s == NULL) {
        return NULL;
    }
    for (req = (SNDSTREAMREQUEST*)s->active.head; req != NULL; req = (SNDSTREAMREQUEST*)req->link.next) {
        if (req->id == request) {
            return req;
        }
    }
    return req;
}
