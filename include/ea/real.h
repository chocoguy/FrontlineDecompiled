// EA "REAL" runtime library (ea/sys, ea/snd): C-linkage functions used by the
// game, declared as decompiled code needs them. Parameter types are inferred
// from call sites unless noted.
#ifndef _EA_REAL_H_
#define _EA_REAL_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Library setup / teardown
int CPU_detect(void);  // returns the core clock
void REAL_restore(void);

// Timer
extern s32 TIMERhz;
s32 TIMER_init(s32 hz);  // 0: 100 Hz; returns the rate
void TIMER_restore(void);

// Threads and synchronous tasks
void THREAD_init(void);
void SYNCTASK_init(void);
int SYNCTASK_run(int arg);
void SYNCTASK_del(void (*task)(void));
void THREAD_yield(int arg);
int THREAD_iscurrent(int thread);
u8 FILE_exists(const char* name);

// Data streams: files or memory images of chunks (8-byte header: LE tag,
// LE size) read into a ring buffer and sorted into tracks by filters. A
// stream handle is its track 1 (STREAM_taphandle gives the others).
int STREAM_overhead(int numrequests, int numfilters, int numtracks);
int STREAM_create(int numrequests, int numfilters, int numtracks, void* buffer, int bufferSize);
void STREAM_destroy(int stream);
// priority: normal reads; greedypriority: reads while the buffer is below
// the greedy level. Lower runs first.
void STREAM_setpriority(int stream, int priority, int greedypriority);
void STREAM_setgreedylevel(int stream, int level);
void STREAM_setgreedystate(int stream, int greedy);
// Chunks whose tag & mask == value go to track dest (-1/-2: dropped); the
// last filter must match everything.
void STREAM_setfilter(int stream, int filter, int mask, int value, int dest);
int STREAM_taphandle(int stream, int track);
// tag: the chunk tag ending the request (0: none); a memory image of size 0
// runs up to that chunk.
int STREAM_queuefile(int stream, const char* name, int offset, int tag);
int STREAM_queuemem(int stream, void* data, int size, int tag);
void STREAM_cancelrequest(int stream, int request);
void STREAM_kill(int stream);
void* STREAM_get(int stream);
void STREAM_release(int stream, void* chunk);
int STREAM_gettable(int stream);  // bytes buffered for the track
int STREAM_state(int stream);  // 0 idle, 1 reading, 2 buffer full
u8 STREAM_isendofstream(int stream);
int STREAM_buffersize(int stream);
int SNDPROFILE_outputlatency(void);

// Controllers (one PADStatus per port, read once a frame by PAD_update)
struct PADStatus;
void PAD_init(void);
struct PADStatus* PAD_getdataptr(int port);
void PAD_update(void);

// Memory
void* MEM_alloc(const char* name, u32 size, s32 flags);
// align 0: the class's block alignment (with flag 0x400 its data alignment); data + offset is aligned.
void* MEM_allocalign(const char* name, u32 size, s32 align, s32 offset, s32 flags);
int MEM_free(void* ptr);
void MEM_fill(void* dest, int value, int size);
void MEM_copy(void* dest, const void* src, int size);
void MEM_clear(void* dest, int size);
int MEM_totalunused(int pool);
void MEM_restore(void);

// Files
typedef void* (*FILESYS_AllocFn)(const char* name, int size, int flags);
typedef void (*FILESYS_FreeFn)(void* ptr);
void FILESYS_setmemcallbacks(FILESYS_AllocFn alloc, FILESYS_FreeFn free);
// numhandles (0: 24), unused (0: 0x800), numops (0: 10)
int FILESYS_init(int numhandles, int unused, int numops);
int FILESYS_initadr(int numhandles, int unused, int numops, void* mem);
int FILESYS_overhead(int numhandles, int unused, int numops);
void FILESYS_restore(void);
// Asynchronous operations: each returns an operation handle (0 on failure)
// to poll (FILESYS_opstatus), wait on, or give a callback, then complete
// (FILESYS_completeop returns the result). File handles are HANDLE_def*.
int FILESYS_opstatus(int op);
void FILESYS_callbackop(int op, void (*callback)(int op, int status, void* data));
void FILESYS_priorityop(int op, int priority);
void FILESYS_cancelop(int op);
int FILESYS_waitop(int op);
int FILESYS_completeop(int op);
int FILESYS_exists(const char* name, int priority, void* data);
int FILESYS_open(const char* name, int mode, int priority, void* data);
int FILESYS_close(int file, int priority, void* data);
int FILESYS_read(int file, int offset, void* buffer, int size, int priority, void* data);
int FILESYS_size(int file, int priority, void* data);
int FILESYS_addbig(const char* name, int memflags, int priority, void* data);
int FILESYS_delbig(int big, int priority, void* data);
// Runs func(priority, data) with only ops up to `priority` allowed to start.
int FILESYS_atomic(int (*func)(int priority, void* data), int unused, int priority, void* data);
// Up to 256 requests; the pool is allocated with memflags.
void ASYNCFILE_init(int numrequests, int memflags);
void ASYNCFILE_restore(void);

// Register/unregister a BIG archive for synchronous reads; returns a handle.
int FILESYS_addbigsync(const char* path, int flags, int priority, int* handle);
int FILESYS_delbigsync(int handle, int priority);
int FILESYS_opensync(const char* path, int mode, int priority, int* handle);
int FILESYS_closesync(int handle, int priority);
int FILESYS_sizesync(int handle, int priority);
int FILESYS_existssync(const char* path, int priority);

// Asynchronous reads: start, poll (1 = done), collect the byte count.
int ASYNCFILE_read(int file, int offset, void* buffer, int size);
int ASYNCFILE_getstatus(int request);
int ASYNCFILE_release(int request, int* status, int* bytes);
// Loads a whole file in the background; ASYNCFILE_release hands back the
// buffer (in *status) and its size.
int ASYNCFILE_load(const char* name, int flags);
int ASYNCFILE_cancel(int request);

// Whole-file helpers (paths are relative to the disc root)
int FILE_sizez(const char* path);
void* FILE_loadz(const char* path, int flags);
void* FILE_loadsize(const char* path, int* size, int flags);
void* FILE_loadbigheader(const char* path, int flags);

// BIG archives (EA .viv): a loaded archive or archive header in memory
int BIG_count(const void* big);
int BIG_typeofheader(const void* header);  // 0: not a BIG header
int BIG_sizeofheader(const void* header);
void* BIG_locate(const void* big, const char* name);
// Finds an entry by name (any case) or, with name NULL, by index; returns
// the entry's name in the header (as an int), 0 if not found.
int BIG_locateentryz(const void* big, const char* name, int index, int* offset, int* size);

int FILESYS_readsync(int handle, int offset, void* buffer, int size, int priority);
int FILE_size(const char* path);
void* MEM_allocz(const char* name, int size, int flags);

// Sound (ea/snd). Handles are ints; errors are negative.
// How a sound plays. size 0x18
struct SNDPLAYOPTS {
    /* 0x00 */ u8 vol;
    /* 0x01 */ u8 unk_0x1;
    /* 0x02 */ u8 key;  // 60: middle C
    /* 0x03 */ u8 unk_0x3;
    /* 0x04 */ u8 drylevel;
    /* 0x05 */ u8 fxlevel;
    /* 0x06 */ u16 unk_0x6;
    /* 0x08 */ u16 azimuth;  // 3d position
    /* 0x0A */ s16 distance;
    /* 0x0C */ u16 pitchmult;
    /* 0x0E */ u16 timemult;
    /* 0x10 */ u16 unk_0x10;
    /* 0x12 */ u16 unk_0x12;
    /* 0x14 */ u16 lowpass;
    /* 0x16 */ u16 highpass;
};
int SNDplaysetdef(SNDPLAYOPTS* opts);
void SNDSYS_add100hzclient(void (*client)(void));
void SNDSYS_remove100hzclient(void (*client)(void));
int SNDvol(int handle, int vol);
int SNDstop(int handle);
int SNDpitchmult(int handle, int mult);
int SNDautovol(int handle, int centiseconds, int vol);
int SNDtimeremaining(int handle);  // centiseconds
int SNDCTRL_getprogvol(int handle);
int SNDbankadd(int* bank, void* data);  // 7: only the header needs keeping
int SNDbankheadersize(int bank);
int SNDbankheadercopy(void* dest, int bank);
int SNDbankremove(int bank);
// `format` starts with the u16 sample rate.
int SNDBANK_patchinfo(int bank, int patch, void* format, void* info, u32* samples);
int SNDBANK_play(int bank, int patch, SNDPLAYOPTS* opts);
int SNDSTRM_create(SNDPLAYOPTS* opts, int numRequests, int size, void* buffer, int bufferSize);
int SNDSTRM_destroy(int stream);
int SNDSTRM_overhead(int numRequests, int size);
// Tap streams: data is pushed in by the caller (movie audio).
int SNDSTRM_overheadtap(int numRequests, int size);
int SNDSTRM_createtap(int format, SNDPLAYOPTS* opts, int numRequests, int size, void* buffer, int bufferSize);
int SNDSTRM_queuerequestid(int stream, int file, int id);
int SNDSYS_inited(void);
void DEBUG_break(void);
int SNDSTRM_queuefile(int stream, int file, char* name, int offset);
int SNDSTRM_queuemem(int stream, int file, char* mem, int offset);
int SNDSTRM_purge(int stream);
int SNDSTRM_vol(int stream, int vol);
int SNDSTRM_pitchmult(int stream, int mult);
int SNDSTRM_autovol(int stream, int ms, int vol);
int SNDSTRM_getprogvol(int stream);
int SNDSTRM_modifyhold(int request, int hold);  // hold 0 releases a held request
// size 0xC
struct SNDSTREAMSTATUS {
    /* 0x0 */ s32 queued;
    /* 0x4 */ s32 unk_0x4;
    /* 0x8 */ s32 unk_0x8;
};
int SNDSTRM_status(int stream, SNDSTREAMSTATUS* status);
// size 0x10
struct SNDREQUESTSTATUS {
    /* 0x0 */ s32 state;  // 1 waiting, 0/2 playing, 3 finished
    /* 0x4 */ s32 played;
    /* 0x8 */ s32 remaining;
    /* 0xC */ s32 position;
};
int SNDSTRM_requeststatus(int request, SNDREQUESTSTATUS* status);

void REAL_abortmessage(const char* fmt, ...);

// Output capabilities/settings (SNDSYSOPTS from 0x20; sndgs keeps a second copy). size 0x98
struct SNDSYSNAME7 {
    u8 b[7];
};
struct SNDSYSCAPS {
    /* 0x00 */ s32 unk_0x0;
    /* 0x04 */ s32 seed;  // random seed
    /* 0x08 */ u16 maxBanks;
    /* 0x0A */ u16 rate;  // output sample rate
    /* 0x0C */ u16 unk_0xC;
    /* 0x0E */ u8 numSwVoices;  // CPU mixed voices (after the DSP voices)
    /* 0x0F */ u8 unk_0xF[4];
    /* 0x13 */ u8 numHwVoices;  // DSP voices (the first voices)
    /* 0x14 */ u8 unk_0x14;     // 1..200
    /* 0x15 */ SNDSYSNAME7 unk_0x15;
    /* 0x1C */ u8 stealEqual;  // voices may be stolen from sounds of equal priority
    /* 0x1D */ u8 unk_0x1D[2];
    /* 0x1F */ u8 unk_0x1F;
    /* 0x20 */ u8 unk_0x20[6];
    /* 0x26 */ u8 initAram;  // the driver initialises ARAM and the ARAM queue
    /* 0x27 */ u8 maxPacketPlayers;
    /* 0x28 */ u8 speakers;  // 1 mono, 2 stereo, 3 surround
    /* 0x29 */ u8 unk_0x29[3];
    /* 0x2C */ u8 unk_0x2C;
    /* 0x2D */ u8 numRenderModes;
    /* 0x2E */ u8 unk_0x2E;
    /* 0x30 */ u16 renderModes[4];
    /* 0x38 */ s32 unk_0x38;
    /* 0x3C */ u32 aramBase;  // sample pool
    /* 0x40 */ s32 aramSize;
    /* 0x44 */ s32 unk_0x44[5];
    /* 0x58 */ u16 speakerAz[4][4];  // [speakers - 1][speaker]: output speaker azimuths, ascending
    /* 0x78 */ s32 unk_0x78[8];
};

// What the platform supports (SNDPLATFORM_outputcaps). size 0x20
struct SNDHWCAPS {
    /* 0x00 */ u16 unk_0x0;
    /* 0x02 */ u16 unk_0x2;
    /* 0x04 */ u8 minSpeakers;
    /* 0x05 */ u8 maxSpeakers;
    /* 0x06 */ u8 maxSwVoices;
    /* 0x07 */ u8 unk_0x7[4];
    /* 0x0B */ u8 maxHwVoices;
    /* 0x0C */ u8 unk_0xC[7];
    /* 0x13 */ u8 numRenderModes;
    /* 0x14 */ u16 renderModes[2];
    /* 0x18 */ u8 unk_0x18[8];
};

// Sound system setup. size 0xD0
struct SNDSYSOPTS {
    /* 0x00 */ SNDHWCAPS hw;
    /* 0x20 */ SNDSYSCAPS caps;
    /* 0xB8 */ int (*aramalloc)(int size);  // replace the driver's ARAM allocator
    /* 0xBC */ void (*aramfree)(unsigned int addr);
    /* 0xC0 */ s32 unk_0xC0;
    /* 0xC4 */ void (*abortmsg)(char* msg);
    /* 0xC8 */ s32 unk_0xC8[2];
};
int SNDSYS_vectortoreal(void);
int SNDSYS_getopts(SNDSYSOPTS* opts);
int SNDSYS_setopts(SNDSYSOPTS* opts);
int SNDSYSI_init(void* heap, int heapSize, int aramSize);
int SNDSYS_restore(void);
// User data reached while playing a sound. size 0x14
struct SNDUSERDATACBINFO {
    /* 0x00 */ s32 m_type;  // 0 bank resolved, 1 chunk data, 2 bank removed, 3 stream header
    /* 0x04 */ void* m_data;
    /* 0x08 */ s32 m_size;
    /* 0x0C */ s32 m_param;
    /* 0x10 */ s32 m_request;  // stream headers: the request id
};
void SNDSYS_adduserdataclient(void (*client)(SNDUSERDATACBINFO*));
void SNDSYS_removeuserdataclient(void (*client)(SNDUSERDATACBINFO*), int);
int SNDmastervol(int vol);

// AEMS (event-driven sound effects). Events are int arrays, the event
// number first. Most entry points are pointers, set when AEMS is linked.
int SNDAEMS_streaminit(void* buffer, int size, int streams);
int SNDAEMS_streamrestore(void);
int SNDAEMS_asyncloadmodulebankdone(void);
extern int (*SNDAEMS_addsystem)(void* system);
extern int (*SNDAEMS_removesystem)(void);
extern int (*SNDAEMS_removemodulebank)(int bank);
extern int (*SNDAEMS_beginevent)(void* event);
extern int (*SNDAEMS_updateevent)(int handle, void* event);
extern int (*SNDAEMS_endevent)(int handle, void* event);
extern int (*SNDAEMS_setglobal)(int global, int value);
extern int (*SNDAEMS_addmodulebank)(void* data, const char* streamFile, int, void* (*alloc)(void*, int, int));
extern int (*SNDAEMS_asyncloadmodulebank)(const char* path, int offset, const char* streamFile, int streamOffset,
                                          void* loadBuffer, int loadBufferSize, void* (*alloc)(int));

#ifdef __cplusplus
}
#endif

#endif
