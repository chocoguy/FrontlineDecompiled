// pathxSND: the PATHX tracks (streams and sound banks) under the PATH audio
// player. Compiled as C++; the PATHX_ API has C linkage.
#include "game/pathxSND.h"
#include "ea/real.h"
#include "ea/snd/sserver.h"
#include "ea/sys/systask.h"
#include "ea/sys/timer.h"
#include "stdio.h"
#include "string.h"

PATHXTRACK* pathxinfo[PATHX_MAXTRACKS];
void* pathabortmsg;
void* pathprintf;
PATHXServiceFn pathService;
PATHXServiceFn pathTimer;

// Bank tracks play one patch, in the first request.
static inline int Voice(PATHXTRACK* track) {
    return track->m_requests[0].m_handle;
}

static inline void RefreshStatus(PATHXTRACK* track) {
    if (track->m_dirty) {
        PATHX_statusall(0);
    }
}

// x rounded up past the next 16-byte boundary (always adds 1..16).
static inline int Align16(int x) {
    return x + (16 - x % 16);
}

int PATHX_ifade(int track, int volume, int ms, char skip) {
    int result = 0;
    PATHXTRACK* t = pathxinfo[track];
    if (!skip) {
        SNDSYS_entercritical();
        t->m_fadeVolume = volume;
        if (t->m_isBank) {
            result = SNDautovol(Voice(t), ms / 10, volume);
        } else {
            result = SNDSTRM_autovol(t->m_handle, ms, volume);
        }
        SNDSYS_leavecritical();
    }
    return result;
}

int PATHX_isynctask(int, int) {
    if (pathService != NULL) {
        pathService();
    }
    return 0;
}

int PATHX_milliseconds(void) {
    return (int)(1000.0f * ((float)TIMER_gettick() / (float)TIMERhz));
}

int PATHX_overheadtrack(int numRequests, int stream) {
    int size = numRequests * (int)sizeof(PATHXREQUEST) + (int)sizeof(PATHXTRACK);
    if (stream > 0) {
        size += SNDSTRM_overhead(numRequests, stream);
    }
    return Align16(size);
}

static void* PATHX_loadbank(const char* file) {
    int size = FILE_size(file);
    if (size == 0) {
        return NULL;
    }
    void* data = MEM_allocz("PATHXbnk", size, 0);
    int handle;
    FILESYS_opensync(file, 1, 0, &handle);
    FILESYS_readsync(handle, 0, data, size, 0);
    FILESYS_closesync(handle, 0);
    return data;
}

int PATHX_createtrack(const char* file, const char* name, SNDPLAYOPTS* opts, int numRequests, int streamSize,
                      void* mem, int memSize, void* bankData) {
    int slot;
    for (slot = 0; slot < PATHX_MAXTRACKS; slot++) {
        if (pathxinfo[slot] == NULL) {
            break;
        }
    }
    if (slot >= PATHX_MAXTRACKS) {
        return PATHX_ERR_NOTRACKS;
    }

    // The track, then its requests, then the stream buffer.
    PATHXTRACK* t = (PATHXTRACK*)mem;
    pathxinfo[slot] = t;
    memset(t, 0, sizeof(PATHXTRACK));
    t->m_handle = -1;
    t->m_patch = -1;
    t->m_numRequests = -1;
    t->m_current = -1;
    t->m_fadeVolume = -1;
    t->m_dirty = 1;
    t->m_requests = (PATHXREQUEST*)(t + 1);
    char* buffer = (char*)(t->m_requests + numRequests);
    buffer += 16 - (buffer - (char*)t) % 16;
    int bufferSize = memSize - (buffer - (char*)t);
    for (int i = 0; i < numRequests; i++) {
        t->m_requests[i].m_handle = -1;
    }

    if (opts != NULL) {
        t->m_opts = *opts;
    } else {
        SNDplaysetdef(&t->m_opts);
    }
    sprintf(t->m_name, "%s", name);

    if (streamSize > 0) {
        t->m_handle = SNDSTRM_create(&t->m_opts, numRequests, streamSize, buffer, bufferSize);
    } else {
        t->m_isBank = 1;
        int bank = -1;
        if (bankData != NULL) {
            t->m_bankData = bankData;
        } else {
            t->m_bankData = PATHX_loadbank(file);
        }
        if (t->m_bankData == NULL) {
            bank = PATHX_ERR_NOBANK;
        } else if (SNDbankadd(&bank, t->m_bankData) == 7) {
            // Only the header has to stay resident.
            void* header = MEM_allocz("PATHXbkh", SNDbankheadersize(bank), 0);
            if (header != NULL) {
                SNDbankheadercopy(header, bank);
                MEM_free(t->m_bankData);
                t->m_bankData = header;
            }
        }
        t->m_handle = bank;
    }

    // A failed track keeps its slot (as the original).
    if (t->m_handle < 0) {
        return t->m_handle;
    }
    t->m_numRequests = numRequests;
    return slot;
}

void PATHX_destroytrack(int track) {
    PATHXTRACK* t = pathxinfo[track];
    if (t->m_isBank) {
        SNDbankremove(t->m_handle);
        if (t->m_bankData != NULL) {
            MEM_free(t->m_bankData);
        }
    } else {
        SNDSTRM_destroy(t->m_handle);
    }
    for (int i = 0; i < PATHX_MAXTRACKS; i++) {
        if (pathxinfo[i] == t) {
            pathxinfo[i] = NULL;
        }
    }
}

int PATHX_getvolume(int track) {
    if (pathxinfo[track] != NULL) {
        RefreshStatus(pathxinfo[track]);
    }
    return pathxinfo[track]->m_volume;
}

int PATHX_readyfornewrequest(int track) {
    if (pathxinfo[track] != NULL) {
        RefreshStatus(pathxinfo[track]);
    }
    return pathxinfo[track]->m_ready;
}

int PATHX_timeremaining(int track, int request) {
    PATHXTRACK* t = pathxinfo[track];
    if (t != NULL) {
        RefreshStatus(t);
    }
    if (request < 0) {
        return t->m_totalLength;
    }
    if (request < t->m_numRequests) {
        PATHXREQUEST* r = &t->m_requests[request];
        if (r->m_handle >= 0) {
            return r->m_remaining;
        }
    }
    return PATHX_ERR_NOSTATUS;
}

void PATHX_statusall(int invalidate) {
    SNDSYS_entercritical();
    for (int slot = 0; slot < PATHX_MAXTRACKS; slot++) {
        PATHXTRACK* t = pathxinfo[slot];
        if (t == NULL) {
            continue;
        }
        if (invalidate) {
            t->m_error = 0;
            t->m_dirty = 1;
            continue;
        }

        int error = PATHX_ERR_NOSTATUS;
        int played = 0;
        int remaining = 0;
        t->m_ready = 1;
        if (t->m_handle >= 0) {
            if (t->m_isBank) {
                if (t->m_patch >= 0 && Voice(t) >= 0) {
                    struct {
                        u16 rate;
                        u8 unk_0x2[0x2];
                    } format;
                    u8 info[0x54];
                    u32 samples;
                    error = SNDBANK_patchinfo(t->m_handle, t->m_patch, &format, info, &samples);
                    if (error >= 0) {
                        int left = SNDtimeremaining(Voice(t));
                        if (left < 0) {
                            // Finished.
                            remaining = 0;
                            t->m_requests[0].m_handle = -1;
                            goto store;
                        }
                        remaining = left * 10;
                        played = (int)(1000.0f * ((float)samples / (float)format.rate)) - remaining;
                    }
                    t->m_requests[0].m_remaining = remaining;
                    t->m_position = t->m_requests[0].m_length;
                    t->m_volume = SNDCTRL_getprogvol(Voice(t));
                    t->m_current = Voice(t);
                }
            } else {
                SNDSTREAMSTATUS status;
                error = SNDSTRM_status(t->m_handle, &status);
                t->m_position = 0;
                if (status.queued >= t->m_numRequests) {
                    t->m_ready = 0;
                }
                for (int i = 0; i < t->m_numRequests; i++) {
                    PATHXREQUEST* r = &t->m_requests[i];
                    r->m_remaining = 0;
                    if (r->m_handle < 0) {
                        continue;
                    }
                    SNDREQUESTSTATUS rs;
                    SNDSTRM_requeststatus(r->m_handle, &rs);
                    switch (rs.state) {
                    case 1:
                        // Waiting: all of it is still to play.
                        r->m_remaining = r->m_length;
                        t->m_totalLength += r->m_length;
                        t->m_position += rs.position;
                        t->m_ready = 0;
                        t->m_current = r->m_handle;
                        break;
                    case 0:
                    case 2:
                        played = rs.played;
                        remaining = rs.remaining;
                        if (played == 0 && remaining == 0) {
                            remaining = r->m_length;
                            played = remaining;
                        }
                        r->m_remaining = remaining;
                        t->m_totalLength += remaining;
                        t->m_position += rs.position;
                        if (rs.position < rs.remaining) {
                            t->m_ready = 0;
                        }
                        t->m_current = r->m_handle;
                        break;
                    default:
                        r->m_handle = -1;
                        break;
                    }
                }
                t->m_volume = SNDSTRM_getprogvol(t->m_handle);
            }
        }
    store:
        t->m_played = played;
        t->m_remaining = remaining;
        t->m_error = error;
        t->m_dirty = 0;
    }
    SNDSYS_leavecritical();
}

int PATHX_status(int track, int* played, int* remaining) {
    PATHXTRACK* t = pathxinfo[track];
    if (t == NULL) {
        return PATHX_ERR_NOSTATUS;
    }
    RefreshStatus(t);
    *played = t->m_played;
    *remaining = t->m_remaining;
    if (t->m_error != 0) {
        return t->m_error;
    }
    return t->m_current;
}

void PATHX_initservice(PATHXServiceFn task, PATHXServiceFn timer) {
    for (int i = 0; i < PATHX_MAXTRACKS; i++) {
        pathxinfo[i] = NULL;
    }
    pathService = task;
    pathTimer = timer;
    SNDSYS_add100hzclient(timer);
    SYNCTASK_add(PATHX_isynctask, 5, 100);
    pathabortmsg = (void*)REAL_abortmessage;
    pathprintf = (void*)printf;
}

void PATHX_endservice(void) {
    if (pathService != NULL) {
        SNDSYS_remove100hzclient(pathTimer);
        SYNCTASK_del((void (*)(void))PATHX_isynctask);
        pathService = NULL;
        pathTimer = NULL;
    }
}

int PATHX_queuefile(int track, int file, char* name, int offset, int length, char volume, int index,
                    int purge) {
    PATHXTRACK* t = pathxinfo[track];
    if (volume >= 0) {
        t->m_opts.vol = volume;
        PATHX_volume(track, volume);
    }

    int result;
    if (t->m_isBank) {
        if (index < 0) {
            index = 0;
        }
        t->m_patch = offset + (index - 1);
        SNDSYS_entercritical();
        result = SNDBANK_play(t->m_handle, t->m_patch, &t->m_opts);
        SNDSYS_leavecritical();
        t->m_requests[0].m_handle = result;
        t->m_requests[0].m_length = length;
        t->m_dirty = 1;
    } else {
        if (purge) {
            SNDSTRM_purge(t->m_handle);
            // Clears all but the last request (as the original).
            for (int i = t->m_numRequests - 1; i > 0; i--) {
                t->m_requests[i - 1].m_handle = -1;
                t->m_requests[i - 1].m_length = -1;
            }
        }
        if (t->m_memData != NULL && offset * 4 < t->m_memDataSize) {
            result = SNDSTRM_queuemem(t->m_handle, file, t->m_memData, offset * 4);
        } else {
            result = SNDSTRM_queuefile(t->m_handle, file, name, offset * 4);
        }
        if (result >= 0) {
            // Newest first.
            for (int i = t->m_numRequests - 1; i > 0; i--) {
                t->m_requests[i] = t->m_requests[i - 1];
            }
            t->m_requests[0].m_handle = result;
            t->m_requests[0].m_length = length;
        }
        t->m_dirty = 1;
    }
    return result;
}

void PATHX_modifyhold(int track, int hold) {
    PATHXTRACK* t = pathxinfo[track];
    if (!t->m_isBank) {
        SNDSTRM_modifyhold(Voice(t), hold);
    }
}

int PATHX_volume(int track, int volume) {
    int result = 0;
    PATHXTRACK* t = pathxinfo[track];
    RefreshStatus(t);
    // Not while a fade is running.
    if (t->m_fadeVolume < 0 || t->m_fadeVolume == t->m_volume) {
        t->m_fadeVolume = -1;
        if (t->m_volume != volume && volume >= 0 && t->m_volume >= 0) {
            SNDSYS_entercritical();
            if (t->m_isBank) {
                SNDvol(Voice(t), volume);
            } else {
                SNDSTRM_vol(t->m_handle, volume);
            }
            SNDSYS_leavecritical();
            t->m_volume = volume;
        }
    } else {
        result = PATHX_ERR_NOSTATUS;
    }
    return result;
}

int PATHX_pause(int track, char pause) {
    PATHXTRACK* t = pathxinfo[track];
    u16 pitch;
    if (pause) {
        pitch = 0;
        t->m_pitch = t->m_opts.pitchmult;
    } else {
        pitch = t->m_pitch;
    }
    SNDSYS_entercritical();
    if (t->m_isBank) {
        SNDpitchmult(Voice(t), pitch);
    } else {
        SNDSTRM_pitchmult(t->m_handle, pitch);
    }
    SNDSYS_leavecritical();
    t->m_dirty = 1;
    return 0;
}

int PATHX_stop(int track) {
    PATHXTRACK* t = pathxinfo[track];
    SNDSYS_entercritical();
    if (t->m_isBank) {
        SNDstop(Voice(t));
    } else {
        SNDSTRM_purge(t->m_handle);
    }
    SNDSYS_leavecritical();
    t->m_dirty = 1;
    return 0;
}
