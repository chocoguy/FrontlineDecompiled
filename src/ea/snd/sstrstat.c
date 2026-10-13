#include "ea/snd/sst.h"

// state: 0 queued, 1 headed (next), 2 playing, 3 finished; times in ms.
extern "C" int SNDSTRM_requeststatus(int request, SNDREQUESTSTATUS* status) {
    SNDSTREAMCHANNEL* s;
    SNDSTREAMREQUEST* req;
    u32 rate;

    status->state = 0;
    status->played = 0;
    status->remaining = 0;
    status->position = 0;
    if (request < 0) {
        return -8;
    }
    s = SNDSTRMI_getstreamptr(request & 0xFF);
    if (s == NULL) {
        return -8;
    }
    req = SNDSTRMI_getrequestptr(request);
    if (req != NULL) {
        if (!req->started) {
            status->state = 0;
        } else {
            if ((SNDSTREAMREQUEST*)s->active.head == req) {
                status->state = 2;
                rate = s->format.rate;
            } else {
                status->state = 1;
                rate = s->newFormat.rate;
            }
            status->played = iSNDdivu64(iSNDmulu64(req->played, 1000), rate);
            status->remaining = iSNDdivu64(iSNDmulu64(req->total - req->played, 1000), rate);
            status->position = req->queued * 1000 / rate;
        }
    } else {
        status->state = 3;
    }
    return 0;
}
