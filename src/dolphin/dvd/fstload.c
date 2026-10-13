#include <dolphin/dvd_priv.h>
#include <string.h>

// Low memory boot info (only the parts the loader fills in).
typedef struct OSBootInfoFst {
    /* 0x00 */ DVDDiskID DVDDiskID;
    /* 0x20 */ u8 pad[0x18];
    /* 0x38 */ void* FSTLocation;
    /* 0x3C */ u32 FSTMaxLength;
} OSBootInfoFst;

static s32 status;
static u8 bb2Buf[0x20 + 31];
static DVDBB2* bb2;
static DVDDiskID* idTmp;

static void cb(s32 result, DVDCommandBlock* block) {
    if (result > 0) {
        switch (status) {
        case 0:
            status = 1;
            DVDReadAbsAsyncForBS(block, bb2, 0x20, 0x420, cb);
            break;
        case 1:
            status = 2;
            DVDReadAbsAsyncForBS(block, bb2->FSTAddress, (s32)((bb2->FSTLength + 31) & ~31), (s32)bb2->FSTPosition,
                                 cb);
            break;
        }
    } else if (result == -1) {
    } else if (result == -4) {
        status = 0;
        DVDReset();
        DVDReadDiskID(block, idTmp, cb);
    }
}

// Reads the disc ID, header and FST itself (used when booted without an apploader).
void __fstLoad(void) {
    OSBootInfoFst* bootInfo;
    DVDDiskID* id;
    u8 idTmpBuf[sizeof(DVDDiskID) + 31];
    static DVDCommandBlock block;
    void* arenaHi;

    arenaHi = OSGetArenaHi();
    bootInfo = (OSBootInfoFst*)0x80000000;

    idTmp = (DVDDiskID*)(((u32)idTmpBuf + 31) & ~31);
    bb2 = (DVDBB2*)(((u32)bb2Buf + 31) & ~31);

    DVDReset();
    DVDReadDiskID(&block, idTmp, cb);
    while (DVDGetDriveStatus() != 0) {
    }

    bootInfo->FSTLocation = bb2->FSTAddress;
    bootInfo->FSTMaxLength = bb2->FSTMaxLength;

    id = &bootInfo->DVDDiskID;
    memcpy(id, idTmp, sizeof(DVDDiskID));
    OSReport("\n");
    OSReport("  Game Name ... %c%c%c%c\n", id->gameName[0], id->gameName[1], id->gameName[2], id->gameName[3]);
    OSReport("  Company ..... %c%c\n", id->company[0], id->company[1]);
    OSReport("  Disk # ...... %d\n", id->diskNumber);
    OSReport("  Game ver .... %d\n", id->gameVersion);
    OSReport("  Streaming ... %s\n", (id->streaming == 0) ? "OFF" : "ON");
    OSReport("\n");

    OSSetArenaHi(bb2->FSTAddress);
}
