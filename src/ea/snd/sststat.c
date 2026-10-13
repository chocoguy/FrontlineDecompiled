#include "ea/snd/sst.h"

// Requests queued, the playing request, and ms buffered (played out by the player, or else
// waiting in STREAM).
extern "C" int SNDSTRM_status(int stream, SNDSTREAMSTATUS* status) {
    SNDSTREAMCHANNEL* s;

    status->unk_0x8 = 0;
    status->unk_0x4 = 0;
    status->queued = 0;
    s = SNDSTRMI_getstreamptr(stream);
    if (s == NULL) {
        return -8;
    }
    status->queued = s->active.count;
    if (status->queued != 0) {
        status->unk_0x4 = ((SNDSTREAMREQUEST*)s->active.head)->id;
        if (s->format.rate != 0) {
            status->unk_0x8 = (u32)(SNDPKTPLAY_framesoutstanding(s->player) * 1000) / s->format.rate;
            if (status->unk_0x8 == 0) {
                SNDSTREAMREQUEST* req = SNDSTRMI_getrequestptr(status->unk_0x4);
                if (req->dataRate != 0) {
                    u32 buffered = STREAM_gettable(s->stream);
                    if (buffered > 4000000) {
                        buffered = 4000000;
                    }
                    status->unk_0x8 = buffered * 1000 / req->dataRate;
                }
            }
        }
    }
    return 0;
}
