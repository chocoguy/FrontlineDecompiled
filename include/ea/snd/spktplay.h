// Packet player: streams of caller-supplied sample frames (movie audio).
#ifndef _EA_SND_SPKTPLAY_H_
#define _EA_SND_SPKTPLAY_H_

#include "ea/snd/sndi.h"

// Frame as queued by SNDPKTPLAY_submit (the caller's frames share 0x4 and 0xC).
struct SNDPKTFRAME {
    /* 0x00 */ u32 counter;  // submit number
    /* 0x04 */ s32 samples : 31;
    /* 0x04 */ s32 flag : 1;
    /* 0x08 */ void* data[4];  // per channel
};

struct SNDPKTSUBMIT {
    /* 0x00 */ u32 unk_0x0;
    /* 0x04 */ u32 samples : 31;
    /* 0x04 */ u32 flag : 1;
    /* 0x08 */ u32 unk_0x8;
    /* 0x0C */ void* data[4];
};

typedef void (*SNDPKTFRAMECB)(void* data, void* param);  // data: the frame's data[0]
typedef void (*SNDPKTFREECB)(int player, int samples, void* param);

// A player, kept in the caller's buffer after the platform's part.
struct SNDPKTPLAY {
    /* 0x00 */ s32 voice;  // handle, or an error
    /* 0x04 */ u32 frameCounter;
    /* 0x08 */ s16 readPos[4];  // per channel
    /* 0x10 */ s16 queued[4];
    /* 0x18 */ s16 numFrames;
    /* 0x1A */ s8 master;    // channel that drives the callbacks
    /* 0x1B */ s8 waiting;   // frames queued for the master channel
    /* 0x1C */ s16 notifyPos;
    /* 0x1E */ s16 writePos;
    /* 0x20 */ s32 samplesQueued;
    /* 0x24 */ s32 samplesPlaying;
    /* 0x28 */ void* buffer;
    /* 0x2C */ SNDPKTFRAMECB frameDone;
    /* 0x30 */ SNDPKTFREECB framesFree;
    /* 0x34 */ void* param;
    /* 0x38 */ u16 rate;
    /* 0x3A */ u8 channels;
    /* 0x3B */ u8 unk_0x3B;
    /* 0x3C */ void* chanData[4];
    /* 0x4C */ SNDPKTFRAME frames[1];
};

// Callbacks collected at interrupt time, run by SNDPKTPLAYI_flushcallbackdata.
struct SNDPKTCALLBACK {
    u16 type;  // 1 frame done, 0 frames freed
    u16 player;
    u32 value;  // frame done: its data[0]; freed: the sample count
};

struct SNDPKTPLAYSTATE {
    /* 0x000 */ s32 numCallbacks;
    /* 0x004 */ SNDPKTCALLBACK callbacks[96];
    /* 0x304 */ SNDPKTPLAY* players[32];
};

OFFSET_ASSERT(SNDPKTPLAY, frameDone, 0x2C);
OFFSET_ASSERT(SNDPKTPLAY, rate, 0x38);
OFFSET_ASSERT(SNDPKTPLAY, frames, 0x4C);
SIZE_ASSERT(SNDPKTFRAME, 0x18);
SIZE_ASSERT(SNDPKTPLAYSTATE, 0x384);

extern SNDPKTPLAYSTATE sndpps;

int SNDPKTPLAYI_get(int player, int chan, int* samples, int* flag);
void SNDPKTPLAYI_freeframes(int player, int chan, int samples);
void SNDPKTPLAYI_flushcallbackdata();
int SNDPKTPLAYI_voicetopackethandle(int voice);

extern "C" {
int SNDPKTPLAY_overhead(int frames);
int SNDPKTPLAY_create(SNDPKTFRAMECB frameDone, SNDPKTFREECB framesFree, void* param, void* buffer, int size);
int SNDPKTPLAY_start(int player, SNDSAMPLEFORMAT* format, SNDSAMPLEATTR* attr, SNDPLAYOPTS* opts);
u32 SNDPKTPLAY_submit(int player, SNDPKTSUBMIT* frame);
int SNDPKTPLAY_submitspace(int player);
int SNDPKTPLAY_framesoutstanding(int player);
int SNDPKTPLAY_stop(int player);
int SNDPKTPLAY_destroy(int player);
}

#endif
