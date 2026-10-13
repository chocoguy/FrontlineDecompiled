#include <dolphin/os.h>
#include <dolphin/os_priv.h>
#include <dolphin/dvd.h>
#include <dolphin/dvd_priv.h>

#define OS_BOOTROM_ADDR 0x81300000
#define APPLOADER_OFFSET 0x2440

#define OS_RESET_CODE (*(u32*)0x817FFFFC)
#define OS_REBOOT_BOOL (*(u32*)0x817FFFF8)

typedef struct ApploaderHeader {
    /* 0x00 */ char date[16];
    /* 0x10 */ u32 entry;
    /* 0x14 */ u32 size;
    /* 0x18 */ u32 rebootSize;
    /* 0x1C */ u32 reserved2;
} ApploaderHeader;

// DMA target, so 32-byte aligned.
static ApploaderHeader Header __attribute__((aligned(32)));

static void* SaveStart = NULL;
static void* SaveEnd = NULL;
static volatile BOOL Prepared;

static void Run(register u32 addr) {
    OSDisableInterrupts();
    ICFlashInvalidate();
    asm {
        sync
        isync
        mtlr addr
        blr
    }
}

static void Callback(void) {
    Prepared = TRUE;
}

// Waits for a read; resets the console on any disc error.
#define WaitRead(block)                                              \
    do {                                                             \
        s32 __state;                                                 \
                                                                     \
        for (;;) {                                                   \
            __state = *(volatile s32*)&(block)->state;               \
            if (__state == 0) {                                      \
                break;                                               \
            }                                                        \
            if (__state == -1 || (__state >= 2 && __state < 12)) {   \
                __OSDoHotReset(OS_RESET_CODE);                       \
            }                                                        \
        }                                                            \
    } while (0)

// Run ends in a hand-written jump, so keep it out of line as in the original.
#pragma dont_inline on
void __OSReboot(u32 resetCode, BOOL forceMenu) {
    OSContext exceptionContext;
    DVDCommandBlock dvdCmd;
    DVDCommandBlock dvdCmd2;
    u32 numBytes;
    u32 offset;

    OSDisableInterrupts();

    OS_RESET_CODE = resetCode;
    OS_REBOOT_BOOL = 0;
    *(u8*)0x800030E2 = TRUE;
    *(void**)0x812FDFF0 = SaveStart;
    *(void**)0x812FDFEC = SaveEnd;

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    DVDInit();
    DVDSetAutoInvalidation(TRUE);

    __DVDPrepareResetAsync((DVDCBCallback)Callback);

    if (!DVDCheckDisk()) {
        __OSDoHotReset(OS_RESET_CODE);
    }

    __OSMaskInterrupts(0xFFFFFFE0);
    __OSUnmaskInterrupts(0x400);  // PI_DI

    OSEnableInterrupts();

    // Read the apploader header, then the reboot code that follows it, to 0x81300000.
    while (!Prepared) {
    }
    DVDReadAbsAsyncForBS(&dvdCmd, &Header, sizeof(ApploaderHeader), APPLOADER_OFFSET, NULL);
    WaitRead(&dvdCmd);

    offset = Header.size + 0x20;
    numBytes = (Header.rebootSize + 31) & ~31;

    while (!Prepared) {
    }
    DVDReadAbsAsyncForBS(&dvdCmd2, (void*)OS_BOOTROM_ADDR, (s32)numBytes, (s32)(offset + APPLOADER_OFFSET), NULL);
    WaitRead(&dvdCmd2);

    ICInvalidateRange((void*)OS_BOOTROM_ADDR, numBytes);
    Run(OS_BOOTROM_ADDR);
}
#pragma dont_inline reset
