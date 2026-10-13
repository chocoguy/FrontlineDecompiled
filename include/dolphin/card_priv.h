// Dolphin SDK memory card library internals, following the public Dolphin SDK.
#ifndef _DOLPHIN_CARD_PRIV_H_
#define _DOLPHIN_CARD_PRIV_H_

#include <dolphin/card.h>
#include <dolphin/dsp.h>
#include <dolphin/dvd.h>
#include <dolphin/exi.h>
#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARD_RESULT_UNLOCKED 1

#define CARD_FAT_CHECKSUM 0x0000u
#define CARD_FAT_CHECKSUMINV 0x0001u
#define CARD_FAT_CHECKCODE 0x0002u
#define CARD_FAT_FREEBLOCKS 0x0003u
#define CARD_FAT_LASTSLOT 0x0004u

#define CARD_SYSTEM_BLOCK_SIZE (8 * 1024u)
#define CARD_NUM_SYSTEM_BLOCK 5
#define CARD_MAX_MOUNT_STEP 7
#define CARD_SEG_SIZE 512u
#define CARD_PAGE_SIZE 128u
#define CARD_ID_SIZE 512

#define CARDFreq EXI_FREQ_16M

#define CARDIsValidBlockNo(card, iBlock) (CARD_NUM_SYSTEM_BLOCK <= (iBlock) && (iBlock) < (card)->cBlock)

// Directory entry. size 0x40
typedef struct CARDDir {
    /* 0x00 */ u8 gameName[4];
    /* 0x04 */ u8 company[2];
    /* 0x06 */ u8 _padding0;
    /* 0x07 */ u8 bannerFormat;
    /* 0x08 */ u8 fileName[CARD_FILENAME_MAX];
    /* 0x28 */ u32 time;
    /* 0x2C */ u32 iconAddr;
    /* 0x30 */ u16 iconFormat;
    /* 0x32 */ u16 iconSpeed;
    /* 0x34 */ u8 permission;
    /* 0x35 */ u8 copyTimes;
    /* 0x36 */ u16 startBlock;
    /* 0x38 */ u16 length;
    /* 0x3A */ u8 _padding1[2];
    /* 0x3C */ u32 commentAddr;
} CARDDir;

// System block 0 (card ID). size 0x200
typedef struct CARDID {
    /* 0x000 */ u8 serial[32];  // flash code, format time, counter bias, language, VI DTV status
    /* 0x020 */ u16 deviceID;
    /* 0x022 */ u16 size;
    /* 0x024 */ u16 encode;  // font encoding
    /* 0x026 */ u8 padding[0x1D6];
    /* 0x1FC */ u16 checkSum;
    /* 0x1FE */ u16 checkSumInv;
} CARDID;

// Trailer of a directory block (after the 127 entries). size 0x40
typedef struct CARDDirCheck {
    /* 0x00 */ u8 padding0[0x3A];
    /* 0x3A */ s16 checkCode;
    /* 0x3C */ u16 checkSum;
    /* 0x3E */ u16 checkSumInv;
} CARDDirCheck;

#define __CARDGetDirCheck(dir) ((CARDDirCheck*)&(dir)[CARD_MAX_FILE])
#define CARD_FAT_AVAIL 0x0000u

// Parameters for the DSP unlock task, at the start of the work area. size 0x10
typedef struct CARDDecParam {
    /* 0x0 */ u8* inputAddr;
    /* 0x4 */ u32 inputLength;
    /* 0x8 */ u32 aramAddr;
    /* 0xC */ u8* outputAddr;
} CARDDecParam;

// Per-slot control block. size 0x110
typedef struct CARDControl {
    /* 0x000 */ BOOL attached;
    /* 0x004 */ s32 result;
    /* 0x008 */ u16 size;
    /* 0x00A */ u16 pageSize;
    /* 0x00C */ s32 sectorSize;
    /* 0x010 */ u16 cBlock;
    /* 0x012 */ u16 vendorID;
    /* 0x014 */ s32 latency;
    /* 0x018 */ u8 id[12];
    /* 0x024 */ union {
        int mountStep;
        int formatStep;  // this SDK version reuses the mount step while formatting
    };
    /* 0x028 */ u32 scramble;
    /* 0x02C */ u32 unk_0x2C;
    /* 0x030 */ DSPTaskInfo task;
    /* 0x080 */ void* workArea;
    /* 0x084 */ CARDDir* currentDir;
    /* 0x088 */ u16* currentFat;
    /* 0x08C */ OSThreadQueue threadQueue;
    /* 0x094 */ u8 cmd[9];
    /* 0x0A0 */ s32 cmdlen;
    /* 0x0A4 */ volatile u32 mode;
    /* 0x0A8 */ int retry;
    /* 0x0AC */ int repeat;
    /* 0x0B0 */ u32 addr;
    /* 0x0B4 */ void* buffer;
    /* 0x0B8 */ s32 xferred;
    /* 0x0BC */ u16 freeNo;
    /* 0x0BE */ u16 startBlock;
    /* 0x0C0 */ CARDFileInfo* fileInfo;
    /* 0x0C4 */ CARDCallback extCallback;
    /* 0x0C8 */ CARDCallback txCallback;
    /* 0x0CC */ CARDCallback exiCallback;
    /* 0x0D0 */ CARDCallback apiCallback;
    /* 0x0D4 */ CARDCallback xferCallback;
    /* 0x0D8 */ CARDCallback eraseCallback;
    /* 0x0DC */ CARDCallback unlockCallback;
    /* 0x0E0 */ OSAlarm alarm;
    /* 0x108 */ u32 cid;
    /* 0x10C */ const DVDDiskID* diskID;
} CARDControl;

extern CARDControl __CARDBlock[2];
extern DVDDiskID __CARDDiskNone;

// CARDBios
void __CARDDefaultApiCallback(s32 chan, s32 result);
void __CARDSyncCallback(s32 chan, s32 result);
void __CARDExtHandler(s32 chan, OSContext* context);
void __CARDExiHandler(s32 chan, OSContext* context);
void __CARDTxHandler(s32 chan, OSContext* context);
void __CARDUnlockedHandler(s32 chan, OSContext* context);
s32 __CARDEnableInterrupt(s32 chan, BOOL enable);
s32 __CARDReadStatus(s32 chan, u8* status);
s32 __CARDClearStatus(s32 chan);
s32 __CARDReadSegment(s32 chan, CARDCallback callback);
s32 __CARDWritePage(s32 chan, CARDCallback callback);
s32 __CARDEraseSector(s32 chan, u32 addr, CARDCallback callback);
void __CARDSetDiskID(const DVDDiskID* id);
s32 __CARDGetControlBlock(s32 chan, CARDControl** pcard);
s32 __CARDPutControlBlock(CARDControl* card, s32 result);
s32 __CARDSync(s32 chan);

// CARDUnlock
s32 __CARDUnlock(s32 chan, u8 flashID[12]);

// CARDRdwr
s32 __CARDRead(s32 chan, u32 addr, u32 length, void* dst, CARDCallback callback);
s32 __CARDWrite(s32 chan, u32 addr, u32 length, void* src, CARDCallback callback);

// CARDBlock
void* __CARDGetFatBlock(CARDControl* card);
s32 __CARDAllocBlock(s32 chan, u32 cBlock, CARDCallback callback);
s32 __CARDFreeBlock(s32 chan, u16 nBlock, CARDCallback callback);
s32 __CARDUpdateFatBlock(s32 chan, u16* fat, CARDCallback callback);

// CARDDir
void* __CARDGetDirBlock(CARDControl* card);
s32 __CARDUpdateDir(s32 chan, CARDCallback callback);

// CARDCheck
void __CARDCheckSum(void* ptr, int length, u16* checksum, u16* checksumInv);
s32 __CARDVerify(CARDControl* card);

// CARDMount
void __CARDMountCallback(s32 chan, s32 result);

// CARDFormat
s32 __CARDFormatRegionAsync(s32 chan, u16 encode, CARDCallback callback);

// CARDOpen
BOOL __CARDCompareFileName(CARDDir* ent, const char* fileName);
s32 __CARDAccess(CARDControl* card, CARDDir* ent);
s32 __CARDIsPublic(CARDDir* ent);
s32 __CARDGetFileNo(CARDControl* card, const char* fileName, s32* pfileNo);
BOOL __CARDIsOpened(CARDControl* card, s32 fileNo);

// CARDRead
s32 __CARDSeek(CARDFileInfo* fileInfo, s32 length, s32 offset, CARDControl** pcard);

// CARDStatEx
s32 __CARDGetStatusEx(s32 chan, s32 fileNo, CARDDir* dirent);
s32 __CARDSetStatusExAsync(s32 chan, s32 fileNo, CARDDir* dirent, CARDCallback callback);

// CARDNet
extern u16 __CARDVendorID;

#ifdef __cplusplus
}
#endif

#endif
