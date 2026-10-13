// Minimal Dolphin SDK memory card declarations, following the public Dolphin SDK.
#ifndef _DOLPHIN_CARD_H_
#define _DOLPHIN_CARD_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CARD_RESULT_READY 0
#define CARD_RESULT_BUSY -1
#define CARD_RESULT_WRONGDEVICE -2
#define CARD_RESULT_NOCARD -3
#define CARD_RESULT_NOFILE -4
#define CARD_RESULT_IOERROR -5
#define CARD_RESULT_BROKEN -6
#define CARD_RESULT_EXIST -7
#define CARD_RESULT_NOENT -8
#define CARD_RESULT_INSSPACE -9
#define CARD_RESULT_NOPERM -10
#define CARD_RESULT_LIMIT -11
#define CARD_RESULT_NAMETOOLONG -12
#define CARD_RESULT_ENCODING -13
#define CARD_RESULT_CANCELED -14
#define CARD_RESULT_FATAL_ERROR -128

#define CARD_MAX_FILE 127
#define CARD_FILENAME_MAX 32
#define CARD_WORKAREA_SIZE (5 * 8 * 1024)

#define CARD_ATTR_PUBLIC 0x04
#define CARD_ATTR_NO_COPY 0x08
#define CARD_ATTR_NO_MOVE 0x10

#define CARD_STAT_ICON_NONE 0
#define CARD_STAT_ICON_C8 1
#define CARD_STAT_BANNER_NONE 0
#define CARD_STAT_ANIM_LOOP 0x00
#define CARD_STAT_SPEED_END 0
#define CARD_STAT_SPEED_SLOW 3

typedef void (*CARDCallback)(s32 chan, s32 result);

typedef struct CARDFileInfo {
    /* 0x0 */ s32 chan;
    /* 0x4 */ s32 fileNo;
    /* 0x8 */ s32 offset;
    /* 0xC */ s32 length;
    /* 0x10 */ u16 iBlock;
    /* 0x12 */ u16 __padding;
} CARDFileInfo;

typedef struct CARDStat {
    /* 0x0 */ char fileName[CARD_FILENAME_MAX];
    /* 0x20 */ u32 length;
    /* 0x24 */ u32 time;
    /* 0x28 */ u8 gameName[4];
    /* 0x2C */ u8 company[2];
    /* 0x2E */ u8 bannerFormat;
    /* 0x2F */ u8 __padding;
    /* 0x30 */ u32 iconAddr;
    /* 0x34 */ u16 iconFormat;
    /* 0x36 */ u16 iconSpeed;
    /* 0x38 */ u32 commentAddr;
    /* 0x3C */ u32 offsetBanner;
    /* 0x40 */ u32 offsetBannerTlut;
    /* 0x44 */ u32 offsetIcon[8];
    /* 0x64 */ u32 offsetIconTlut;
    /* 0x68 */ u32 offsetData;
} CARDStat;

#define CARDSetBannerFormat(stat, f) ((stat)->bannerFormat = (u8)(((stat)->bannerFormat & ~0x03) | (f)))
#define CARDSetIconAnim(stat, f) ((stat)->bannerFormat = (u8)(((stat)->bannerFormat & ~0x04) | (f)))
#define CARDSetIconFormat(stat, n, f) \
    ((stat)->iconFormat = (u16)(((stat)->iconFormat & ~(0x03 << (2 * (n)))) | ((f) << (2 * (n)))))
#define CARDSetIconSpeed(stat, n, f) \
    ((stat)->iconSpeed = (u16)(((stat)->iconSpeed & ~(0x03 << (2 * (n)))) | ((f) << (2 * (n)))))
#define CARDSetIconAddress(stat, addr) ((stat)->iconAddr = (u32)(addr))
#define CARDSetCommentAddress(stat, addr) ((stat)->commentAddr = (u32)(addr))

void CARDInit(void);
s32 CARDProbe(s32 chan);
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback);
s32 CARDUnmount(s32 chan);
s32 CARDGetResultCode(s32 chan);
s32 CARDCheckAsync(s32 chan, CARDCallback callback);
s32 CARDCheckExAsync(s32 chan, s32* xferBytes, CARDCallback callback);
s32 CARDFormatAsync(s32 chan, CARDCallback callback);
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed);
s32 CARDGetSectorSize(s32 chan, u32* size);
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo);
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo);
s32 CARDClose(CARDFileInfo* fileInfo);
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback);
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback);
s32 CARDDelete(s32 chan, const char* fileName);
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* addr, s32 length, s32 offset, CARDCallback callback);
s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* addr, s32 length, s32 offset, CARDCallback callback);
s32 CARDWrite(CARDFileInfo* fileInfo, const void* addr, s32 length, s32 offset);
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat);
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback);
s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat);
s32 CARDSetAttributes(s32 chan, s32 fileNo, u8 attr);

#ifdef __cplusplus
}
#endif

#endif
