// Booting another executable from the disc: reads the ELF's header and
// program segments into place (retrying on drive errors), then resets and
// jumps to its entry point. The block at 0x80003100 survives the switch.
#include "game/loadelf.h"
#include "game/dvdStatus_base.h"
#include "dolphin/dvd.h"
#include "dolphin/os.h"
#include "dolphin/gx.h"
#include "dolphin/vi.h"
#include "stdio.h"
#include "string.h"

// ELF header fields used.
struct ElfHeader {
    /* 0x00 */ u32 magic;  // 0x7F 'E' 'L' 'F'
    /* 0x04 */ u8 unk_0x4[0x14];
    /* 0x18 */ void (*entry)(void);
    /* 0x1C */ u32 phoff;
    /* 0x20 */ u8 unk_0x20[0xC];
    /* 0x2C */ s16 phnum;
};

// A program segment. size 0x20
struct ElfSegment {
    /* 0x00 */ u32 type;
    /* 0x04 */ u32 offset;
    /* 0x08 */ void* vaddr;
    /* 0x0C */ u32 paddr;
    /* 0x10 */ u32 filesz;
    /* 0x14 */ u32 memsz;
    /* 0x18 */ u32 flags;
    /* 0x1C */ u32 align;
};

static u8 theCodePointer[0x1020];  // the header, 32-byte aligned inside
static int HadError;

LoadElf* LoadElf::Get() {
    return (LoadElf*)0x80003100;
}

// Reads `length` bytes at `offset`, retrying until it completes; -1 once a
// drive error has been seen.
static inline int ReadFile(DVDFileInfo* file, void* addr, int length, int offset) {
    bool done;
    do {
        HadError += CheckDVDError();
        DVDReadAsyncPrio(file, addr, length, offset, NULL, 2);
        s32 status;
        do {
            status = DVDGetCommandBlockStatus(&file->cb);
            if (status == 0) {
                done = true;
            } else {
                HadError += CheckDVDError();
                if (HadError != 0) {
                    DVDCancel(&file->cb);
                    done = false;
                    break;
                }
            }
            if (HadError != 0) {
                return -1;
            }
        } while (status != 0);
        if (HadError != 0) {
            return -1;
        }
    } while (!done);
    return length;
}

void LoadElf::RunModule(const char* path) {
    DVDFileInfo file;
    ElfHeader* header = (ElfHeader*)(((u32)theCodePointer + 0x1F) & ~0x1F);
    void (*entry)(void);
    int errors = 0;

    do {
        HadError = 0;
        printf("LOADER: Now parsing and loading %s\n", path);
        HadError += CheckDVDError();
        errors += HadError;
        s32 entrynum;
        do {
            entrynum = DVDConvertPathToEntrynum(path);
            HadError += CheckDVDError();
        } while (entrynum == -1);
        errors += HadError;
        if (HadError != 0) {
            continue;
        }
        BOOL opened;
        do {
            opened = DVDFastOpen(entrynum, &file);
            HadError += CheckDVDError();
        } while (!opened);
        errors += HadError;
        if (HadError != 0) {
            continue;
        }
        int read = ReadFile(&file, header, 0x1000, 0);
        errors += HadError;
        if (HadError != 0) {
            continue;
        }
        if (read != 0x1000) {
            printf("LOADER: DVDRead failed loading ELF header\n");
            errors++;
            HadError = 1;
            continue;
        }
        if (header->magic != 0x7F454C46) {
            printf("LOADER: Fatal Error: ELF Magic number not present in header\n");
            errors++;
            HadError = 1;
            continue;
        }

        entry = header->entry;
        int numSegments = header->phnum;
        ElfSegment* segment = (ElfSegment*)((u8*)header + header->phoff);
        for (int i = 0; i < numSegments; i++) {
            void* addr = segment->vaddr;
            u32 memsz = segment->memsz;
            int size = (segment->filesz + 0x1F) & ~0x1F;
            printf("LOADER: Now loading segment %d at %08x, size %08x\n", i, addr, memsz);
            memset(addr, 0, memsz);
            if (size > 0) {
                HadError += CheckDVDError();
                errors += HadError;
                if (HadError != 0) {
                    continue;  // as the original: without moving to the next segment
                }
                if (ReadFile(&file, addr, size, segment->offset) != size) {
                    printf("LOADER: DVDRead failed loading segment %d\n", i);
                    HadError = 1;
                    break;
                }
            }
            printf("LOADER: Clearing I and D cache\n");
            ICInvalidateRange(addr, size);
            DCFlushRange(addr, size);
            segment++;
        }
        DVDClose(&file);
        if (HadError != 0) {
            printf("LOADER: Had DVD error processing ELF, reloading...\n");
            errors = 1;
        }
    } while (HadError != 0);

    if (errors != 0) {
        printf("LOADER: Calling AppCLeanup due to DVD error\n");
        if (gGxIsInit) {
            GXDrawDone();
            VISetBlack(1);
            VIFlush();
            VIWaitForRetrace();
        }
    }
    PPCSync();
    printf("LOADER: Calling entry point at %08x\n", entry);
    OSResetSystem(2, OSGetResetCode(), 0);
    entry();
}
