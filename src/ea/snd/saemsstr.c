#include "ea/snd/aems.h"

void SNDAEMSI_streamsystemtask();
void SNDAEMSI_updateplayerstream(AEMSCOMPDYNAMICPLAYER* d);
void SNDAEMSI_streamupdateattributes(AEMSCOMPDYNAMICPLAYER* d);

static inline int AEMS_clampu16(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 0xFFFF) {
        return 0xFFFF;
    }
    return v;
}

static inline int AEMS_clamp7fff(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 0x7FFF) {
        return 0x7FFF;
    }
    return v;
}

// Split `buffer` into `numStreams` sound streams for AEMS stream players. Returns each
// stream's data space.
extern "C" int SNDAEMS_streaminit(void* buffer, int size, int numStreams) {
    SNDPLAYOPTS opts;
    int each;
    int i;

    SNDAEMSI_linkstream();
    if (sndaems.streams != NULL) {
        SNDAEMS_streamrestore();
    }
    SNDSYS_entercritical();
    sndaems.streams = (AEMSSTREAMSYS*)SNDMEMI_allocz(0x14);
    sndaems.streams->buffer = (u8*)buffer;
    sndaems.streams->size = size;
    sndaems.streams->numStreams = numStreams;
    sndaems.streams->frames = 0x3C;
    sndaems.streams->streams = (AEMSSTREAMSTATUS**)SNDMEMI_allocz(numStreams << 2);
    SNDplaysetdef(&opts);
    each = (size / numStreams) & 0x7FFFFFFC;
    for (i = 0; i < numStreams; i++) {
        AEMSSTREAMSTATUS* s;
        sndaems.streams->streams[i] = (AEMSSTREAMSTATUS*)SNDMEMI_allocz(0x28);
        s = sndaems.streams->streams[i];
        s->buffer = (u8*)buffer + i * each;
        s->stream = SNDSTRM_create(&opts, 2, 0x3C, s->buffer, each);
        s->owner = NULL;
        s->req.state = 0;
        s->req.hold = -1;
        s->req.offset = -1;
        s->req.offset2 = -1;
        s->req.file = NULL;
    }
    iSNDserveraddclient(SNDAEMSI_streamsystemtask);
    SNDSYS_leavecritical();
    return each - SNDSTRM_overhead(2, 0x3C);
}

void SNDAEMSI_streamdying(AEMSSTREAMSTATUS* s);

extern "C" int SNDAEMS_streamrestore(void) {
    int i;

    if (sndaems.streams == NULL) {
        return 0;
    }
    iSNDserverremoveclient(SNDAEMSI_streamsystemtask);
    for (i = 0; i < sndaems.streams->numStreams; i++) {
        AEMSSTREAMSTATUS* s = sndaems.streams->streams[i];
        if (s->owner != NULL) {
            SNDAEMSI_streamdying(s);
        }
        SNDSTRM_destroy(s->stream);
        SNDSYS_entercritical();
        SNDMEMI_free(s);
        SNDSYS_leavecritical();
    }
    SNDSYS_entercritical();
    SNDMEMI_free(sndaems.streams->streams);
    SNDMEMI_free(sndaems.streams);
    SNDSYS_leavecritical();
    sndaems.streams = NULL;
    return 0;
}

// A free stream, or the lowest priority one if it is below `priority` (its player is cut off).
int SNDAEMSI_streamgethandle(AEMSCOMPDYNAMICPLAYER* d, int priority) {
    AEMSSTREAMSYS* sys = sndaems.streams;
    int best = 100;
    int bestIndex = -1;
    int i;

    if (sys == NULL) {
        return -14;
    }
    for (i = 0; i < sys->numStreams; i++) {
        AEMSSTREAMSTATUS* s = sys->streams[i];
        if (s->owner == NULL && s->req.state == 0) {
            s->owner = d;
            s->priority = priority;
            return s->stream;
        }
    }
    for (i = 0; i < sys->numStreams; i++) {
        if (sys->streams[i]->priority < best) {
            best = sys->streams[i]->priority;
            bestIndex = i;
        }
    }
    if (priority > best && bestIndex >= 0) {
        AEMSSTREAMSTATUS* s = sys->streams[bestIndex];
        if (s->owner != NULL) {
            SNDAEMSI_streamdying(s);
        }
        SNDSTRM_purge(s->stream);
        s->owner = d;
        s->priority = priority;
        return s->stream;
    }
    return -1;
}

// The stream is taken away from its player.
void SNDAEMSI_streamdying(AEMSSTREAMSTATUS* s) {
    SNDSYS_entercritical();
    if (s->owner == NULL) {
        SNDSYS_leavecritical();
        return;
    }
    s->req.state = 0;
    s->owner->out = 0;
    s->owner->timeLeft = 0;
    s->owner->position = 0;
    s->owner->handle = -1;
    s->owner->stream = -1;
    s->owner->req = NULL;
    SNDSYS_leavecritical();
}

// Follow the player's stream requests: playing, finished, time left and position.
void SNDAEMSI_streamupdatestatus(AEMSCOMPDYNAMICPLAYER* d) {
    static const SNDREQUESTSTATUS zero = {0, 0, 0, 0};
    AEMSSTREAMSTATUS* s = NULL;
    SNDREQUESTSTATUS status;
    SNDREQUESTSTATUS status2 = zero;
    int i;

    for (i = 0; i < sndaems.streams->numStreams; i++) {
        s = sndaems.streams->streams[i];
        if (s->stream == d->stream) {
            break;
        }
    }
    if (i >= sndaems.streams->numStreams) {
        return;
    }
    if (s->owner == NULL) {
        return;
    }
    if (d->handle == 0x7FFFFFFF) {
        d->out = 0;
        d->timeLeft = 0;
        d->position = 0;
        d->handle = -1;
        d->stream = -1;
        s->owner = NULL;
        s->req.player = NULL;
        s->req.state = 0;
        d->req = NULL;
        return;
    }
    if (s->req.state == 2 || s->req.state == 1) {
        return;
    }
    SNDSTRM_requeststatus(d->handle, &status);
    if (d->handle2 >= 0) {
        SNDSTRM_requeststatus(d->handle2, &status2);
    }
    if (status.state == 3) {
        d->out = 0;
        d->timeLeft = 0;
        d->position = 0;
        d->handle = -1;
        d->handle2 = -1;
        s->owner = NULL;
        s->req.state = 0;
        d->req = NULL;
        SNDSTRM_purge(d->stream);
        d->stream = -1;
    } else if (status.state == 2) {
        d->out = 1;
        if (d->req != NULL && s->req.state == 3) {
            s->req.state = 2;
        } else {
            d->timeLeft = status.remaining;
            d->position = status.played;
        }
    } else if (status.state == 0 || status.state == 1) {
        d->out = 1;
        d->timeLeft = 0x7FFFFFFF;
        if (d->req != NULL && d->handle2 >= 0 && status2.state == 2) {
            d->position = status2.played;
        } else {
            d->position = 0;
        }
    }
}

// Player for streams: on the gate's rising edge get a stream and queue the file part(s);
// 0 stops (2 holds); while playing follow the inputs.
void SNDAEMSI_updateplayerstream(AEMSCOMPDYNAMICPLAYER* d) {
    AEMSCOMP* comp = d->comp;

    if (d->lastGate != d->in[7]) {
        int gate = d->in[7];
        if (gate == 1) {
            if (d->lastGate == 0) {
                AEMSPLAYERTABLE* table = (AEMSPLAYERTABLE*)comp->p[0];
                MODULEBANK* bank;
                AEMSSTREAMENTRY* entry;
                int index = d->in[6];
                if (index >= table->count) {
                    index = table->count - 1;
                }
                d->entry = table->entries[index];
                bank = (MODULEBANK*)comp->p[1];
                entry = &bank->streams[d->entry & 0x3FFF];
                d->stream = SNDAEMSI_streamgethandle(d, entry->priority);
                d->handle = -1;
                if (d->stream >= 0) {
                    AEMSSTREAMREQ* req = NULL;
                    int i;
                    for (i = 0; i < sndaems.streams->numStreams; i++) {
                        AEMSSTREAMSTATUS* s = sndaems.streams->streams[i];
                        if (s->stream == d->stream) {
                            req = &s->req;
                            break;
                        }
                    }
                    req->player = d;
                    req->hold = entry->hold;
                    req->file = bank->rt->streamFile;
                    req->offset = bank->rt->streamOffset + entry->offset;
                    if (entry->flags & 1) {
                        req->state = 2;
                        req->offset2 = bank->rt->streamOffset + entry->offset2;
                        d->req = req;
                    } else {
                        req->state = 1;
                        req->offset2 = -1;
                        d->req = NULL;
                    }
                    d->out = 1;
                    d->timeLeft = 0x7FFFFFFF;
                    d->position = 0;
                    d->pitch = AEMS_clampu16(d->in[0]);
                    SNDSTRM_pitchmult(d->stream, d->pitch);
                    d->vol = AEMS_clampu16(d->in[2]);
                    SNDSTRM_vol(d->stream, d->vol >> 8);
                    d->azimuth = d->in[3];
                    if (d->in[4] < -0x4000) {
                        d->distance = -0x4000;
                    } else if (d->in[4] > 0x3FFF) {
                        d->distance = 0x3FFF;
                    } else {
                        d->distance = d->in[4];
                    }
                    SNDSTRM_3dpos(d->stream, d->azimuth, d->distance);
                    d->fx = AEMS_clamp7fff(d->in[5]);
                    SNDSTRM_fxlevel(d->stream, 0, d->fx >> 8);
                    d->lowpass = AEMS_clampu16(d->in[8]);
                    SNDSTRM_lowpass(d->stream, d->lowpass);
                    d->highpass = AEMS_clampu16(d->in[9]);
                    SNDSTRM_highpass(d->stream, d->highpass);
                    d->timemult = AEMS_clampu16(d->in[1]);
                    SNDSTRM_timemult(d->stream, d->timemult);
                    if (((MODULEBANK*)comp->p[1])->version >= 9) {
                        d->dry = AEMS_clamp7fff(d->in[10]);
                        SNDSTRM_drylevel(d->stream, (s8)(d->dry >> 8));
                    }
                } else {
                    d->out = 0;
                    d->timeLeft = 0;
                    d->position = 0;
                    if (d->req != NULL) {
                        d->req->state = 0;
                        d->req->player = NULL;
                    }
                    d->req = NULL;
                }
            }
        } else if (gate == 2) {
            if (d->lastGate != 1) {
                d->in[7] = 0;
            }
        } else if (gate == 0) {
            if (d->stream > -1) {
                SNDSTRM_purge(d->stream);
                if (d->handle < 0) {
                    d->handle = 0x7FFFFFFF;
                }
                SNDAEMSI_streamupdatestatus(d);
            }
        }
        d->lastGate = d->in[7];
    }
    if (d->lastGate == 1 && d->stream > -1) {
        SNDAEMSI_streamupdateattributes(d);
    }
}

// Server client: queue the requests the players asked for (outside the critical section,
// dropping them if the stream changed hands meanwhile).
void SNDAEMSI_streamsystemtask() {
    int i;

    if (sndaems.streams == NULL) {
        return;
    }
    SNDSYS_entercritical();
    for (i = 0; i < sndaems.streams->numStreams; i++) {
        AEMSSTREAMSTATUS* s = sndaems.streams->streams[i];
        AEMSCOMPDYNAMICPLAYER* player;
        int first;
        int handle;

        if (s->req.state == 0 || s->req.state == 3) {
            continue;
        }
        player = s->req.player;
        s->owner = player;
        first = player->handle;
        SNDSYS_leavecritical();
        handle = SNDSTRM_queuefile(player->stream, s->req.hold, s->req.file, s->req.offset);
        SNDSYS_entercritical();
        if (player != s->owner) {
            continue;
        }
        if (s->req.offset2 > -1) {
            first = handle;
            SNDSYS_leavecritical();
            handle = SNDSTRM_queuefile(player->stream, s->req.hold, s->req.file, s->req.offset2);
            SNDSYS_entercritical();
            if (player != s->owner) {
                continue;
            }
            s->req.offset = s->req.offset2;
            s->req.offset2 = -1;
        }
        player->handle = handle;
        player->handle2 = first;
        if (s->req.state == 2) {
            s->req.state = 3;
        } else {
            s->req.state = 0;
        }
        player->out = 1;
        player->timeLeft = 0x7FFFFFFF;
        player->position = 0;
    }
    SNDSYS_leavecritical();
}

void SNDAEMSI_streamupdateattributes(AEMSCOMPDYNAMICPLAYER* d) {
    AEMSCOMP* comp = d->comp;

    SNDAEMSI_streamupdatestatus(d);
    if (d->stream == -1) {
        return;
    }
    if (d->in[0] != d->pitch) {
        d->pitch = AEMS_clampu16(d->in[0]);
        SNDSTRM_pitchmult(d->stream, d->pitch);
    }
    if (d->in[2] != d->vol) {
        d->vol = AEMS_clamp7fff(d->in[2]);
        SNDSTRM_vol(d->stream, d->vol >> 8);
    }
    if (d->in[3] != d->azimuth || d->in[4] != d->distance) {
        d->azimuth = d->in[3];
        if (d->in[4] < -0x4000) {
            d->distance = -0x4000;
        } else if (d->in[4] > 0x4000) {
            d->distance = 0x4000;
        } else {
            d->distance = d->in[4];
        }
        SNDSTRM_3dpos(d->stream, d->azimuth, d->distance);
    }
    if (d->in[5] != d->fx) {
        d->fx = AEMS_clamp7fff(d->in[5]);
        SNDSTRM_fxlevel(d->stream, 0, d->fx >> 8);
    }
    if (d->in[8] != d->lowpass) {
        d->lowpass = AEMS_clampu16(d->in[8]);
        SNDSTRM_lowpass(d->stream, d->lowpass);
    }
    if (d->in[9] != d->highpass) {
        d->highpass = AEMS_clampu16(d->in[9]);
        SNDSTRM_highpass(d->stream, d->highpass);
    }
    if (d->in[1] != d->timemult) {
        d->timemult = AEMS_clampu16(d->in[1]);
        SNDSTRM_timemult(d->stream, d->timemult);
    }
    if (((MODULEBANK*)comp->p[1])->version >= 9 && d->in[10] != d->dry) {
        d->dry = AEMS_clamp7fff(d->in[10]);
        SNDSTRM_drylevel(d->stream, (s8)(d->dry >> 8));
    }
}

void SNDAEMSI_linkstream() {
    sndaems.updatePlayerStream = SNDAEMSI_updateplayerstream;
}
