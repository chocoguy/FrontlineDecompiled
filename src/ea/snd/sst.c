#include "ea/snd/sst.h"
#include <string.h>

SNDSTREAMCHANNEL* sndss[32];

static inline SNDSTREAMCHANNEL* SNDSTRMI_streamptr(int stream) {
    if (stream >= sndgs.opts.caps.maxPacketPlayers || stream < 0) {
        return NULL;
    }
    return sndss[stream];
}

// Bytes per second of a sample format (by compression type).
int SNDSTRMI_calcdatarate(SNDSAMPLEFORMAT* format) {
    int rate = format->rate * format->channels;
    int mult = 0;
    if (format->unk_0x3 == 10) {
        mult = 0x88;
    } else if (format->unk_0x3 == 4) {
        mult = 0x33;
    } else if (format->unk_0x3 == 7) {
        mult = 0x200;
    } else if (format->unk_0x3 == 16) {
        return format->channels * 8;
    }
    return (rate * mult) >> 8;
}

int SNDSTRMI_destroyall() {
    int i;
    for (i = 0; i < sndgs.opts.caps.maxPacketPlayers; i++) {
        SNDSTRM_destroy(i);
    }
    return 0;
}

SNDSTREAMCHANNEL* SNDSTRMI_getstreamptr(int stream) {
    if (stream >= sndgs.opts.caps.maxPacketPlayers || stream < 0) {
        return NULL;
    }
    return sndss[stream];
}

static inline void SNDSTRMI_retire(SNDSTREAMREQUEST* req) {
    SNDSTREAMCHANNEL* s = sndss[req->id & 0xFF];
    SNDSTREAMREQUEST* r = SNDSTRMI_getrequestptr(req->id);
    SNDLINKI_remove(&s->active, &r->link);
    SNDLINKI_push(&s->free, &r->link);
    if (s->current == r) {
        s->current = NULL;
    }
}

// Frame played: give its chunk (stored just before the data) back to STREAM.
void SNDSTRMI_releasecallback(void* data, void* param) {
    STREAMCHUNKHDR* chunk = ((STREAMCHUNKHDR**)data)[-1];
    STREAM_release(sndss[chunk->tag & 0xFF]->stream, chunk);
}

// Samples played: credit the oldest requests, retiring each one that is finished.
void SNDSTRMI_framescallback(int player, int samples, void* param) {
    SNDSTREAMCHANNEL* s = (SNDSTREAMCHANNEL*)param;
    int extra = 0;
    int count = 0;

    for (;;) {
        SNDSTREAMREQUEST* req;
        count++;
        req = (SNDSTREAMREQUEST*)s->active.head;
        if (count > 200) {
            SNDSTRMI_retire(req);
            return;
        }
        if ((u32)samples > req->queued) {
            extra = samples - req->queued;
            samples -= extra;
        }
        req->played += samples;
        req->queued -= samples;
        if (req->played >= req->total) {
            SNDSTRMI_retire(req);
        }
        if (extra == 0) {
            return;
        }
        samples = extra;
        extra = 0;
    }
}

// Header of the next request: format, user data; restart the player if the format changed.
int SNDSTRMI_parseheader(int stream, STREAMCHUNKHDR* chunk) {
    SNDSTREAMCHANNEL* s = sndss[stream];
    SNDSTREAMREQUEST* req;
    SNDSAMPLEDESC desc;
    int i;

    if (s->current == NULL) {
        s->current = (SNDSTREAMREQUEST*)s->active.head;
    } else {
        s->current = (SNDSTREAMREQUEST*)s->current->link.next;
    }
    req = s->current;
    SNDI_patchtohdr(NULL, (TAGGEDPATCH*)((u8*)chunk + 8), &s->newFormat, &s->newAttr, &desc);
    req->total = desc.samples;
    req->started = 0;
    for (i = 0; s->newAttr.userData[i] != NULL; i++) {
        SNDUSERDATACBINFO info;
        int j;
        info.m_type = 3;
        info.m_data = s->newAttr.userData[i];
        info.m_size = s->newAttr.userDataSize[i];
        info.m_request = req->id;
        s->newAttr.userData[i] = NULL;
        s->newAttr.userDataSize[i] = 0;
        for (j = 0; j < sndgs.numUserDataClients; j++) {
            sndgs.userDataClients[j](&info);
        }
    }
    STREAM_release(s->stream, chunk);
    req->dataRate = SNDSTRMI_calcdatarate(&s->newFormat);
    if (memcmp(&s->format, &s->newFormat, sizeof(SNDSAMPLEFORMAT)) != 0 ||
        memcmp(&s->attr, &s->newAttr, sizeof(SNDSAMPLEATTR)) != 0 || s->newAttr.data[0] != NULL) {
        if (s->format.rate != 0) {
            s->state = 2;
            return 0;
        }
        s->format = s->newFormat;
        s->attr = s->newAttr;
        s->newAttr.data[0] = NULL;
    }
    if (s->state != 1) {
        s->voice = SNDPKTPLAY_start(s->player, &s->format, &s->attr, &s->opts);
        if (s->filter.unk_0x10 != NULL) {
            SNDCTRL_filteradd(s->voice, &s->filter);
        }
        s->state = 1;
    }
    return 0;
}

// Queue a data chunk's frame to the player; the chunk is released when the frame is done.
void SNDSTRMI_parsedata(SNDSTREAMCHANNEL* s, STREAMCHUNKHDR* chunk) {
    SNDPKTSUBMIT frame;
    SNDSTREAMREQUEST* req;
    u8* base;
    int i;

    frame.samples = CHUNK_WORD(chunk->samples);
    base = (u8*)&chunk->offsets[0] + s->format.channels * 4;
    for (i = 0; i < s->format.channels; i++) {
        frame.data[i] = base + CHUNK_WORD(chunk->offsets[i]);
    }
    req = s->current;
    if (frame.samples == 0) {
        STREAM_release(s->stream, chunk);
        return;
    }
    ((STREAMCHUNKHDR**)frame.data[0])[-1] = chunk;
    chunk->tag = req->id;
    req->queued += frame.samples;
    frame.flag = req->started;
    SNDPKTPLAY_submit(s->player, &frame);
    req->started = 1;
}

// A request on hold waits until `hold` ms of data are buffered (or the buffer can't grow).
int SNDSTRMI_isheld(SNDSTREAMCHANNEL* s) {
    SNDSTREAMREQUEST* req = s->current;
    u32 buffered;

    if (req == NULL) {
        return 0;
    }
    if (req->dataRate == 0) {
        return 0;
    }
    if (req->hold < 0) {
        return 1;
    }
    if (req->hold == 0) {
        return 0;
    }
    buffered = STREAM_gettable(s->stream);
    if (buffered > 4000000) {
        buffered = 4000000;
    }
    if (buffered * 1000 / req->dataRate >= (u32)req->hold) {
        req->hold = 0;
        return 0;
    }
    if (STREAM_state(s->stream) == 2) {
        req->hold = 0;
        return 0;
    }
    if (s->free.count <= 0 && STREAM_state(s->stream) == 0) {
        req->hold = 0;
        return 0;
    }
    return 1;
}

// Server client: feed each stream's player from its STREAM.
void SNDSTRMI_service() {
    int i;

    SNDSYS_entercritical();
    for (i = 0; i < sndgs.opts.caps.maxPacketPlayers; i++) {
        SNDSTREAMCHANNEL* s = sndss[i];
        int n;
        int more;

        if (s == NULL || s->active.count == 0) {
            continue;
        }
        if (s->state == 2) {
            if (SNDPKTPLAY_framesoutstanding(s->player) > 0) {
                continue;
            }
            s->format = s->newFormat;
            s->attr = s->newAttr;
            s->newAttr.data[0] = NULL;
            SNDPKTPLAY_stop(s->player);
            s->voice = SNDPKTPLAY_start(s->player, &s->format, &s->attr, &s->opts);
            if (s->filter.unk_0x10 != NULL) {
                SNDCTRL_filteradd(s->voice, &s->filter);
            }
            s->state = 1;
        }
        if (SNDSTRMI_isheld(s)) {
            continue;
        }
        if (s->state == 1) {
            n = SNDPKTPLAY_submitspace(s->player);
            if (n == 0) {
                continue;
            }
        } else {
            n = 10;
        }
        more = 0;
        do {
            STREAMCHUNKHDR* chunk;
            n--;
            chunk = (STREAMCHUNKHDR*)STREAM_get(s->stream);
            if (chunk != NULL) {
                u32 tag = CHUNK_WORD(chunk->tag);
                if (tag == STREAMTAG_DATA) {
                    SNDSTRMI_parsedata(sndss[i], chunk);
                    more = 1;
                } else if (tag == STREAMTAG_HEADER) {
                    SNDSTRMI_parseheader(i, chunk);
                    more = 0;
                } else {
                    STREAM_release(sndss[i]->stream, chunk);
                    more = 1;
                }
            }
        } while (more != 0 && n > 0);
    }
    SNDSYS_leavecritical();
}

static inline int SNDSTRMI_numstreams() {
    int count = 0;
    int i;
    for (i = 0; i < sndgs.opts.caps.maxPacketPlayers; i++) {
        if (sndss[i] != NULL) {
            count++;
        }
    }
    return count;
}

int SNDSTRMI_create(SNDPLAYOPTS* opts, int numRequests, int frames, void* buffer, int size, int tap, int isTap) {
    SNDSTREAMCHANNEL* s;
    u8* requests;
    u8* player;
    u8* data;
    int index;
    int i;

    for (index = 0; index < sndgs.opts.caps.maxPacketPlayers; index++) {
        if (sndss[index] == NULL) {
            break;
        }
    }
    if (index >= sndgs.opts.caps.maxPacketPlayers) {
        return -9;
    }

    s = (SNDSTREAMCHANNEL*)buffer;
    size -= sizeof(SNDSTREAMCHANNEL);
    memset(s, 0, sizeof(SNDSTREAMCHANNEL));
    requests = (u8*)buffer + sizeof(SNDSTREAMCHANNEL);
    player = requests + numRequests * sizeof(SNDSTREAMREQUEST);
    size -= numRequests * sizeof(SNDSTREAMREQUEST);
    data = player + SNDPKTPLAY_overhead(frames);
    size -= SNDPKTPLAY_overhead(frames);
    SNDLINKI_init(&s->active);
    SNDLINKI_init(&s->free);
    for (i = 0; i < numRequests; i++) {
        SNDLINKI_push(&s->free, (SNDLINKNODE*)requests);
        requests += sizeof(SNDSTREAMREQUEST);
    }
    s->player = SNDPKTPLAY_create(SNDSTRMI_releasecallback, SNDSTRMI_framescallback, s, player,
                                  SNDPKTPLAY_overhead(frames));
    if (s->player < 0) {
        return s->player;
    }
    if (isTap) {
        s->stream = tap;
        s->tap = 1;
    } else {
        int stream = STREAM_create(numRequests + 2, 1, 1, data, size);
        if (stream == 0) {
            SNDPKTPLAY_destroy(s->player);
            return -9;
        }
        s->stream = stream;
        s->tap = 0;
        SNDSTRM_setgreedylevel(s->stream, STREAM_buffersize(s->stream) / 3);
    }
    s->requestSeq = 0;
    s->voice = -1;
    s->opts = *opts;
    if (SNDSTRMI_numstreams() == 0) {
        iSNDserveraddclient(SNDSTRMI_service);
        sndgs.restoreHook6 = (void (*)(void))SNDSTRMI_destroyall;
    }
    sndss[index] = s;
    s->newAttr.data[0] = NULL;
    SNDSTRM_purge(index);
    return index;
}

// type 0: file `name` at `offset`, 1: memory image at `name`, else `offset` is a STREAM request.
int SNDSTRMI_queue(int stream, int hold, char* name, int offset, int type) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_streamptr(stream);
    SNDSTREAMREQUEST* req;
    int handle;
    int id;

    if (s == NULL) {
        return -8;
    }
    if (s->free.count == 0) {
        return -13;
    }
    if (type == 0) {
        handle = STREAM_queuefile(s->stream, name, offset, 0x6C454353);
    } else if (type == 1) {
        handle = STREAM_queuemem(s->stream, name, 0, 0x6C454353);
    } else {
        handle = offset;
    }
    if (handle == 0) {
        return -1;
    }
    SNDSYS_entercritical();
    req = (SNDSTREAMREQUEST*)SNDLINKI_pop(&s->free);
    memset(req, 0, sizeof(SNDSTREAMREQUEST));
    SNDLINKI_pushtail(&s->active, &req->link);
    req->streamRequest = handle;
    s->requestSeq += 0x100;
    if (s->requestSeq < 0) {
        s->requestSeq = 0;
    }
    req->id = s->requestSeq | stream;
    req->hold = hold;
    id = req->id;
    SNDSYS_leavecritical();
    return id;
}

extern "C" int SNDSTRM_create(SNDPLAYOPTS* opts, int numRequests, int frames, void* buffer, int size) {
    return SNDSTRMI_create(opts, numRequests, frames, buffer, size, 0, 0);
}

extern "C" int SNDSTRM_destroy(int stream) {
    SNDSTREAMCHANNEL* s = SNDSTRMI_streamptr(stream);
    if (s == NULL) {
        return -8;
    }
    SNDSTRM_purge(stream);
    if (SNDSTRMI_numstreams() == 1) {
        iSNDserverremoveclient(SNDSTRMI_service);
        sndgs.restoreHook6 = NULL;
    }
    SNDPKTPLAY_destroy(s->player);
    sndss[stream] = NULL;
    if (s->tap == 0) {
        STREAM_destroy(s->stream);
    }
    return 0;
}

extern "C" int SNDSTRM_queuefile(int stream, int hold, char* name, int offset) {
    return SNDSTRMI_queue(stream, hold, name, offset, 0);
}

// Stop the stream and drop all its requests.
extern "C" int SNDSTRM_purge(int stream) {
    SNDSTREAMCHANNEL* s;
    SNDLINKNODE* node;
    int i;

    SNDSYS_entercritical();
    s = SNDSTRMI_streamptr(stream);
    if (s == NULL) {
        SNDSYS_leavecritical();
        return -8;
    }
    if (s->voice >= 0) {
        SNDPKTPLAY_stop(s->player);
    }
    s->voice = -1;
    if (s->tap == 0) {
        STREAM_kill(s->stream);
    }
    if (s->newAttr.data[0] != NULL) {
        for (i = 0; i < s->newFormat.channels; i++) {
            SNDMEMI_free(s->newAttr.data[i]);
        }
    }
    do {
        node = SNDLINKI_pop(&s->active);
        if (node != NULL) {
            SNDLINKI_push(&s->free, node);
        }
    } while (node != NULL);
    s->current = NULL;
    s->state = 0;
    memset(&s->format, 0, sizeof(SNDSAMPLEFORMAT));
    memset(&s->newFormat, 0, sizeof(SNDSAMPLEFORMAT));
    memset(&s->attr, 0, sizeof(SNDSAMPLEATTR));
    memset(&s->newAttr, 0, sizeof(SNDSAMPLEATTR));
    SNDSYS_leavecritical();
    return 0;
}
