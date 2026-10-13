// Asynchronous bank loading: the file is read in blocks; the header goes to memory from the
// caller's allocator, the sample data to ARAM.
#ifndef _EA_SND_SBASYNC_H_
#define _EA_SND_SBASYNC_H_

#include "ea/snd/sndi.h"

struct SNDBANKASYNCBLOCK {
    /* 0x0 */ s32 op;        // FILESYS read
    /* 0x4 */ s32 download;  // SNDPLATFORM_download handle
    /* 0x8 */ u8* buffer;
    /* 0xC */ s8 state;      // 0 free, 1 reading, 2 read, 3 downloading
};

// Load state, at the start of the caller's buffer (the block buffers follow from 0xC0).
struct SNDBANKASYNC {
    /* 0x00 */ u8 active;  // asyncservice is a server client
    /* 0x04 */ s32 openOp;
    /* 0x08 */ s32 closeOp;
    /* 0x0C */ BANKVER5* header;
    /* 0x10 */ void* (*alloc)(int size);
    /* 0x14 */ s32 offset;  // of the bank in the file
    /* 0x18 */ s32 file;
    /* 0x1C */ s32 partial;  // header bytes at the start of the block being downloaded
    /* 0x20 */ s32 total;
    /* 0x24 */ s32 headerLeft;
    /* 0x28 */ u8* aram;
    /* 0x2C */ s32 aramPos;
    /* 0x30 */ s32 sampleLeft;
    /* 0x34 */ u16 blockSize;
    /* 0x36 */ u16 bank;
    /* 0x38 */ u16 readsIssued;
    /* 0x3A */ u16 readsDone;
    /* 0x3C */ u16 consumed;    // blocks copied or sent to ARAM
    /* 0x3E */ u16 downloaded;  // blocks finished (free again)
    /* 0x40 */ SNDBANKASYNCBLOCK blocks[4];
};

struct SNDBANKASYNCSTATE {
    SNDBANKASYNC* cur;
    s32 priority;
    s32 result;
};

extern SNDBANKASYNCSTATE sndbas;

extern "C" {
void SNDI_memcpy(void* dest, const void* src, int size);
int SNDBANK_asyncload(const char* name, int offset, void* buffer, int size, void* (*alloc)(int));
int SNDBANK_asyncdone(void);
}

#endif
