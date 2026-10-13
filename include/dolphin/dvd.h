// Minimal Dolphin SDK DVD declarations, following the public Dolphin SDK.
#ifndef _DOLPHIN_DVD_H_
#define _DOLPHIN_DVD_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVDCommandBlock DVDCommandBlock;
typedef void (*DVDCBCallback)(s32 result, DVDCommandBlock* block);

struct DVDCommandBlock {
    DVDCommandBlock* next;
    DVDCommandBlock* prev;
    u32 command;
    s32 state;
    u32 offset;
    u32 length;
    void* addr;
    u32 currTransferSize;
    u32 transferredSize;
    void* id;
    DVDCBCallback callback;
    void* userData;
};  // size 0x30

typedef struct DVDFileInfo DVDFileInfo;
typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

struct DVDFileInfo {
    DVDCommandBlock cb;
    u32 startAddr;
    u32 length;
    DVDCallback callback;
};  // size 0x3C

typedef struct DVDDiskID {
    /* 0x00 */ char gameName[4];
    /* 0x04 */ char company[2];
    /* 0x06 */ u8 diskNumber;
    /* 0x07 */ u8 gameVersion;
    /* 0x08 */ u8 streaming;
    /* 0x09 */ u8 streamingBufSize;
    /* 0x0A */ u8 padding[22];
} DVDDiskID;  // size 0x20

void DVDInit(void);
s32 DVDGetDriveStatus(void);  // -1 fatal, 0 ready, 1 busy, 4 no disc, 5 cover open, 6 wrong disc, 11 retry
BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo);
BOOL DVDClose(DVDFileInfo* fileInfo);
s32 DVDConvertPathToEntrynum(const char* pathPtr);
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo);
BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback, s32 prio);
s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block);
s32 DVDCancel(DVDCommandBlock* block);

#ifdef __cplusplus
}
#endif

#endif
