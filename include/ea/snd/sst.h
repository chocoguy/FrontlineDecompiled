// Sound streams (SNDSTRM_*): EA STREAM data played through a packet player.
#ifndef _EA_SND_SST_H_
#define _EA_SND_SST_H_

#include "ea/snd/spktplay.h"

// Stream chunk ('SCHl' header with a tagged patch at +8, 'SCDl' data).
struct STREAMCHUNKHDR {
    /* 0x00 */ u32 tag;  // replaced by the request id once queued to the player
    /* 0x04 */ u32 size;
    /* 0x08 */ u32 samples;  // data chunks
    /* 0x0C */ u32 offsets[1];  // per channel, from the end of the offsets
};

#define STREAMTAG_HEADER 0x5343486C  // 'SCHl'
#define STREAMTAG_DATA 0x5343446C    // 'SCDl'

// One queued file or memory image. size 0x28
struct SNDSTREAMREQUEST {
    /* 0x00 */ SNDLINKNODE link;
    /* 0x08 */ s32 streamRequest;  // STREAM request handle
    /* 0x0C */ s32 id;             // sequence << 8 | stream
    /* 0x10 */ u32 dataRate;       // bytes per second
    /* 0x14 */ u32 played;         // samples
    /* 0x18 */ u32 total;
    /* 0x1C */ u32 queued;
    /* 0x20 */ s32 hold;  // ms to buffer before playing; < 0: hold until released
    /* 0x24 */ u8 started;
};

// A stream, at the start of the caller's buffer (requests, the packet player and the
// STREAM buffer follow). size 0x110
struct SNDSTREAMCHANNEL {
    /* 0x000 */ s32 stream;  // STREAM handle (or the tap's)
    /* 0x004 */ s32 voice;
    /* 0x008 */ s32 player;
    /* 0x00C */ s32 requestSeq;
    /* 0x010 */ u8 state;  // 1 playing, 2 waiting to restart in a new format
    /* 0x011 */ s8 tap;
    /* 0x012 */ u8 unk_0x12[0x2];
    /* 0x014 */ SNDSAMPLEFORMAT format;
    /* 0x018 */ SNDSAMPLEFORMAT newFormat;
    /* 0x01C */ SNDSAMPLEATTR attr;
    /* 0x070 */ SNDSAMPLEATTR newAttr;
    /* 0x0C4 */ SNDPLAYOPTS opts;
    /* 0x0DC */ SNDFILTERDEF filter;
    /* 0x0F4 */ SNDLINKLIST active;
    /* 0x100 */ SNDLINKLIST free;
    /* 0x10C */ SNDSTREAMREQUEST* current;  // request whose data is being parsed
};

SIZE_ASSERT(SNDSTREAMREQUEST, 0x28);
SIZE_ASSERT(SNDSTREAMCHANNEL, 0x110);
OFFSET_ASSERT(SNDSTREAMCHANNEL, format, 0x14);
OFFSET_ASSERT(SNDSTREAMCHANNEL, newAttr, 0x70);
OFFSET_ASSERT(SNDSTREAMCHANNEL, opts, 0xC4);
OFFSET_ASSERT(SNDSTREAMCHANNEL, filter, 0xDC);
OFFSET_ASSERT(SNDSTREAMCHANNEL, active, 0xF4);
OFFSET_ASSERT(SNDSTREAMCHANNEL, current, 0x10C);

extern SNDSTREAMCHANNEL* sndss[32];

int SNDSTRMI_calcdatarate(SNDSAMPLEFORMAT* format);
int SNDSTRMI_destroyall();
SNDSTREAMCHANNEL* SNDSTRMI_getstreamptr(int stream);
SNDSTREAMREQUEST* SNDSTRMI_getrequestptr(int request);
int SNDSTRMI_create(SNDPLAYOPTS* opts, int numRequests, int frames, void* buffer, int size, int tap, int isTap);
int SNDSTRMI_queue(int stream, int hold, char* name, int offset, int type);

extern "C" {
int SNDSTRM_setgreedylevel(int stream, int level);
}

#endif
