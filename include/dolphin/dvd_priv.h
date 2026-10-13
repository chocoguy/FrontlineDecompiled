// Dolphin SDK DVD library internals, following the public Dolphin SDK.
#ifndef _DOLPHIN_DVD_PRIV_H_
#define _DOLPHIN_DVD_PRIV_H_

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __DIRegs ((volatile u32*)0xCC006000)
#define DI_STATUS 0
#define DI_COVER_STATUS 1
#define DI_CMDBUF0 2
#define DI_CMDBUF1 3
#define DI_CMDBUF2 4
#define DI_MAR 5
#define DI_LENGTH 6
#define DI_CR 7
#define DI_IMMBUF 8
#define DI_CONFIG 9

typedef void (*DVDLowCallback)(u32 intType);

// dvdlow
void __DVDInitWA(void);
void __DVDInterruptHandler(__OSInterrupt interrupt, OSContext* context);
BOOL DVDLowRead(void* addr, u32 length, u32 offset, DVDLowCallback callback);
BOOL DVDLowSeek(u32 offset, DVDLowCallback callback);
BOOL DVDLowWaitCoverClose(DVDLowCallback callback);
BOOL DVDLowReadDiskID(DVDDiskID* diskID, DVDLowCallback callback);
BOOL DVDLowStopMotor(DVDLowCallback callback);
BOOL DVDLowRequestError(DVDLowCallback callback);
BOOL DVDLowInquiry(void* info, DVDLowCallback callback);
BOOL DVDLowAudioStream(u32 subcmd, u32 length, u32 offset, DVDLowCallback callback);
BOOL DVDLowRequestAudioStatus(u32 subcmd, DVDLowCallback callback);
BOOL DVDLowAudioBufferConfig(BOOL enable, u32 size, DVDLowCallback callback);
void DVDLowReset(void);
BOOL DVDLowBreak(void);
DVDLowCallback DVDLowClearCallback(void);
void __DVDLowSetWAType(u32 type, u32 seekLoc);

DVDDiskID* DVDGetCurrentDiskID(void);

// dvdfs
extern OSThreadQueue __DVDThreadQueue;
extern u32 __DVDLongFileNameFlag;
void __DVDFSInit(void);

// dvd
BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback,
                         s32 prio);
BOOL DVDGetCurrentDir(char* path, u32 maxlen);
BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback);
BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback);
void DVDReset(void);
BOOL DVDSetAutoInvalidation(BOOL autoInval);
BOOL DVDCheckDisk(void);
void __DVDPrepareResetAsync(DVDCBCallback callback);

// Disc header block 2 (at 0x420). size 0x20
typedef struct DVDBB2 {
    /* 0x00 */ u32 bootFilePosition;
    /* 0x04 */ u32 FSTPosition;
    /* 0x08 */ u32 FSTLength;
    /* 0x0C */ u32 FSTMaxLength;
    /* 0x10 */ void* FSTAddress;
    /* 0x14 */ u32 userPosition;
    /* 0x18 */ u32 userLength;
    /* 0x1C */ u32 padding0;
} DVDBB2;

// dvdqueue
void __DVDClearWaitingQueue(void);
BOOL __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block);
DVDCommandBlock* __DVDPopWaitingQueue(void);
BOOL __DVDCheckWaitingQueue(void);
BOOL __DVDDequeueWaitingQueue(DVDCommandBlock* block);

// dvderror
void __DVDStoreErrorCode(u32 error);

// fstload
void __fstLoad(void);

#ifdef __cplusplus
}
#endif

#endif
