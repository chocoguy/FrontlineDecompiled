// pathxSND: EA's PATHX layer, the sound-library side of the PATH audio
// player (path.c). A track is either a stream (SNDSTRM, files queued one
// request at a time) or a sound bank (SNDBANK, one patch played at a time).
// Tracks live in caller-supplied memory and are addressed by slot (0..23).
// Hand-maintained.
#ifndef _GAME_PATHXSND_H_
#define _GAME_PATHXSND_H_

#include "types.h"
#include "game_types.h"
#include "ea/real.h"

enum {
    PATHX_MAXTRACKS = 24,
    PATHX_ERR_NOSTATUS = -8,  // no such track/request, or a fade is running
    PATHX_ERR_NOTRACKS = -13,
    PATHX_ERR_NOBANK = -39,
};

// One queued request (newest first). size 0xC
struct PATHXREQUEST {
    /* 0x0 */ s32 m_length;  // ms, from the caller
    /* 0x4 */ s32 m_handle;  // stream request / bank voice, -1 none
    /* 0x8 */ s32 m_remaining;  // ms left
};

// size 0x68, followed in the same memory by the requests and the stream
// buffer (16-byte aligned).
struct PATHXTRACK {
    /* 0x0 */ s32 m_handle;  // stream, or bank
    /* 0x4 */ s32 m_patch;  // bank patch playing
    /* 0x8 */ s32 m_fadeVolume;  // fade target, -1 none
    /* 0xC */ u16 m_pitch;  // saved by PATHX_pause
    /* 0xE */ s8 m_isBank;
    /* 0xF */ char m_name[9];
    /* 0x18 */ void* m_bankData;
    /* 0x1C */ char* m_memData;  // requests inside it are queued from memory
    /* 0x20 */ s32 m_memDataSize;
    /* 0x24 */ SNDPLAYOPTS m_opts;
    // Status, refreshed by PATHX_statusall when m_dirty.
    /* 0x3C */ s32 m_played;  // ms
    /* 0x40 */ s32 m_remaining;  // ms
    /* 0x44 */ s32 m_position;
    /* 0x48 */ s32 m_totalLength;  // ms; only ever added to
    /* 0x4C */ s32 m_ready;  // room for another request
    /* 0x50 */ s32 m_current;  // request playing
    /* 0x54 */ s32 m_volume;
    /* 0x58 */ s32 m_dirty;
    /* 0x5C */ s32 m_error;
    /* 0x60 */ s32 m_numRequests;
    /* 0x64 */ PATHXREQUEST* m_requests;
};
SIZE_ASSERT(PATHXTRACK, 0x68);

typedef void (*PATHXServiceFn)(void);

#ifdef __cplusplus
extern "C" {
#endif

int PATHX_stop(int track);  // 0x800E883C
int PATHX_pause(int track, char pause);  // 0x800E88A8
int PATHX_volume(int track, int volume);  // 0x800E8940
void PATHX_modifyhold(int track, int hold);  // 0x800E8A1C
// Queues `file` (streams) or plays patch `offset + index - 1` (banks).
// `volume` < 0 keeps the volume; `purge` drops what is queued first.
int PATHX_queuefile(int track, int file, char* name, int offset, int length, char volume, int index,
                    int purge);  // 0x800E8A60
void PATHX_endservice(void);  // 0x800E8E64
void PATHX_initservice(PATHXServiceFn task, PATHXServiceFn timer);  // 0x800E8EAC
// Played/remaining ms of the current request; returns the error or the request.
int PATHX_status(int track, int* played, int* remaining);  // 0x800E8F6C
// `invalidate` marks every track for refreshing instead.
void PATHX_statusall(int invalidate);  // 0x800E9000
// `request` < 0: the whole track.
int PATHX_timeremaining(int track, int request);  // 0x800E9310
int PATHX_readyfornewrequest(int track);  // 0x800E93AC
int PATHX_getvolume(int track);  // 0x800E940C
void PATHX_destroytrack(int track);  // 0x800E946C
// `streamSize` > 0 makes a stream track, else a bank track (from
// `bankData`, or loaded from `file`). Returns the slot or an error.
int PATHX_createtrack(const char* file, const char* name, SNDPLAYOPTS* opts, int numRequests, int streamSize,
                      void* mem, int memSize, void* bankData);  // 0x800E9574
// Memory a track needs.
int PATHX_overheadtrack(int numRequests, int stream);  // 0x800E99F0
int PATHX_milliseconds(void);  // 0x800E9A4C

#ifdef __cplusplus
}
#endif

int PATHX_isynctask(int, int);  // 0x800E9AB8
// Fades to `volume` over `ms`; `skip` does nothing.
int PATHX_ifade(int track, int volume, int ms, char skip);  // 0x800E9AEC

extern PATHXTRACK* pathxinfo[PATHX_MAXTRACKS];  // 0x801F7410
extern void* pathabortmsg;  // 0x8034ECA8
extern void* pathprintf;  // 0x8034ECAC
extern PATHXServiceFn pathService;  // 0x8034ECB0
extern PATHXServiceFn pathTimer;  // 0x8034ECB4

#endif
