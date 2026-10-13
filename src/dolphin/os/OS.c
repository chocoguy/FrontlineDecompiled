#include <dolphin/os.h>
#include <dolphin/os_priv.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/dvd.h>
#include <dolphin/dvd_priv.h>
#include <dolphin/exi.h>
#include <dolphin/si.h>
#include <string.h>

#define __PIRegs ((volatile u32*)0xCC003000)
#define __DIRegs ((volatile u32*)0xCC006000)

#define OS_CONSOLE_RETAIL1 0x00000001
#define OS_CONSOLE_EMULATOR 0x10000000
#define OS_CONSOLE_PC_EMULATOR 0x10000001
#define OS_CONSOLE_ARTHUR 0x10000002
#define OS_CONSOLE_MINNOW 0x10000003
#define OS_CONSOLE_DEVHW1 0x10000004
#define OS_CONSOLE_DEVELOPMENT 0x10000000

#define OS_DBINTERFACE_ADDR 0x40
#define OS_DBJUMPPOINT_ADDR 0x60
#define OS_EXCEPTIONTABLE_ADDR 0x3000
#define OS_BI2_ADDR (*(void**)0x800000F4)
#define OS_DEBUG_FLAG_SAVE (*(u8*)0x800030E8)
#define OS_PAD_SPEC_SAVE (*(u8*)0x800030E9)

// Region preserved across OSResetSystem(OS_RESET_RESTART) (see OSReboot.c).
#define OS_SAVED_REGION_START (*(u32*)0x812FDFF0)
#define OS_SAVED_REGION_END (*(u32*)0x812FDFEC)

// Boot info at physical 0. size 0x40
typedef struct OSBootInfo {
    /* 0x00 */ DVDDiskID diskID;
    /* 0x20 */ u32 magic;
    /* 0x24 */ u32 version;
    /* 0x28 */ u32 memorySize;
    /* 0x2C */ u32 consoleType;
    /* 0x30 */ void* arenaLo;
    /* 0x34 */ void* arenaHi;
    /* 0x38 */ void* FSTLocation;
    /* 0x3C */ u32 FSTMaxLength;
} OSBootInfo;

extern u8 __ArenaHi[];
extern u8 __ArenaLo[];
extern u8 _stack_addr[];

extern u32 __PADSpec;

void DBPrintf(char* format, ...);
BOOL __DBIsExceptionMarked(__OSException exception);
void EnableMetroTRKInterrupts(void);

void __OSEVStart(void);
void __OSEVEnd(void);
void __OSEVSetNumber(void);
void __DBVECTOR(void);
void __OSDBINTSTART(void);
void __OSDBJUMPSTART(void);
void __OSDBJUMPEND(void);

static OSBootInfo* BootInfo;
static volatile u32* BI2DebugFlag;
static u32 BI2DebugFlagHolder;
static BOOL AreWeInitialized;
static __OSExceptionHandler* OSExceptionTable;

BOOL __OSInIPL;
OSTime __OSStartTime;

static void OSExceptionInit(void);
static void OSDefaultExceptionHandler(register __OSException exception, register OSContext* context);

#define GET_CONSOLE_TYPE() \
    ((BootInfo == NULL || BootInfo->consoleType == 0) ? OS_CONSOLE_ARTHUR : BootInfo->consoleType)

u32 OSGetConsoleType(void) {
    if (BootInfo == NULL || BootInfo->consoleType == 0) {
        return OS_CONSOLE_ARTHUR;
    }
    return BootInfo->consoleType;
}

// Zeroes the arena, except the region a restarting program asked to keep.
static void ClearArena(void) {
    if (OSGetResetCode() != 0x80000000) {
        memset(OSGetArenaLo(), 0, (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
        return;
    }

    if (OS_SAVED_REGION_START == 0) {
        memset(OSGetArenaLo(), 0, (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
        return;
    }

    if ((u32)OSGetArenaLo() < OS_SAVED_REGION_START) {
        if ((u32)OSGetArenaHi() <= OS_SAVED_REGION_START) {
            memset(OSGetArenaLo(), 0, (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
            return;
        }

        memset(OSGetArenaLo(), 0, OS_SAVED_REGION_START - (u32)OSGetArenaLo());
        if ((u32)OSGetArenaHi() > OS_SAVED_REGION_END) {
            memset((void*)OS_SAVED_REGION_END, 0, (u32)OSGetArenaHi() - OS_SAVED_REGION_END);
        }
    }
}

// ClearArena and OSExceptionInit stay out of line, as in the original.
#pragma dont_inline on
void OSInit(void) {
    void* bi2;
    u32 consoleType;

    if (AreWeInitialized == FALSE) {
        AreWeInitialized = TRUE;

        __OSStartTime = __OSGetSystemTime();
        OSDisableInterrupts();

        BI2DebugFlag = NULL;
        BootInfo = (OSBootInfo*)0x80000000;
        __DVDLongFileNameFlag = 0;

        bi2 = OS_BI2_ADDR;
        if (bi2) {
            BI2DebugFlag = (u32*)((u8*)bi2 + 0xC);
            __PADSpec = *(u32*)((u8*)bi2 + 0x24);
            OS_DEBUG_FLAG_SAVE = (u8)*BI2DebugFlag;
            OS_PAD_SPEC_SAVE = (u8)__PADSpec;
        } else if (BootInfo->arenaHi) {
            BI2DebugFlagHolder = (u32)OS_DEBUG_FLAG_SAVE;
            BI2DebugFlag = &BI2DebugFlagHolder;
            __PADSpec = (u32)OS_PAD_SPEC_SAVE;
        }

        __DVDLongFileNameFlag = 1;

        OSSetArenaLo((BootInfo->arenaLo == NULL) ? __ArenaLo : BootInfo->arenaLo);

        if (BootInfo->arenaLo == NULL && BI2DebugFlag && *BI2DebugFlag < 2) {
            OSSetArenaLo((void*)(((u32)_stack_addr + 0x1F) & ~0x1F));
        }

        OSSetArenaHi((BootInfo->arenaHi == NULL) ? __ArenaHi : BootInfo->arenaHi);

        OSExceptionInit();
        __OSInitSystemCall();
        OSInitAlarm();
        __OSModuleInit();
        __OSInterruptInit();
        __OSSetInterruptHandler(22, __OSResetSWInterruptHandler);  // __OS_INTERRUPT_PI_RSW
        __OSContextInit();
        __OSCacheInit();
        EXIInit();
        SIInit();
        __OSInitSram();
        __OSThreadInit();
        __OSInitAudioSystem();

        PPCMthid2(PPCMfhid2() & ~0x40000000);

        if ((BootInfo->consoleType & OS_CONSOLE_DEVELOPMENT) != 0) {
            BootInfo->consoleType = OS_CONSOLE_DEVHW1;
        } else {
            BootInfo->consoleType = OS_CONSOLE_RETAIL1;
        }
        BootInfo->consoleType += (__PIRegs[11] & 0xF0000000) >> 28;

        if (__OSInIPL == FALSE) {
            __OSInitMemoryProtection();
        }

        OSReport("\nDolphin OS $Revision: 49 $.\n");
        OSReport("Kernel built : %s %s\n", "Dec 17 2001", "18:46:45");
        OSReport("Console Type : ");

        consoleType = GET_CONSOLE_TYPE();
        if ((consoleType & OS_CONSOLE_DEVELOPMENT) == 0) {
            OSReport("Retail %d\n", consoleType);
        } else {
            switch (consoleType) {
            case OS_CONSOLE_EMULATOR:
                OSReport("Mac Emulator\n");
                break;
            case OS_CONSOLE_PC_EMULATOR:
                OSReport("PC Emulator\n");
                break;
            case OS_CONSOLE_ARTHUR:
                OSReport("EPPC Arthur\n");
                break;
            case OS_CONSOLE_MINNOW:
                OSReport("EPPC Minnow\n");
                break;
            default:
                OSReport("Development HW%d\n", (consoleType - OS_CONSOLE_DEVHW1) + 1);
                break;
            }
        }

        OSReport("Memory %d MB\n", BootInfo->memorySize >> 20);
        OSReport("Arena : 0x%x - 0x%x\n", OSGetArenaLo(), OSGetArenaHi());

        if (BI2DebugFlag && *BI2DebugFlag >= 2) {
            EnableMetroTRKInterrupts();
        }

        ClearArena();
        OSEnableInterrupts();
    }
}

#pragma dont_inline reset

static u32 __OSExceptionLocations[] = {
    0x00000100,  // system reset
    0x00000200,  // machine check
    0x00000300,  // DSI
    0x00000400,  // ISI
    0x00000500,  // external interrupt
    0x00000600,  // alignment
    0x00000700,  // program
    0x00000800,  // FP unavailable
    0x00000900,  // decrementer
    0x00000C00,  // system call
    0x00000D00,  // trace
    0x00000F00,  // performance monitor
    0x00001300,  // instruction breakpoint
    0x00001400,  // system management interrupt
    0x00001700,  // thermal
};

// Copies the exception vector template to every vector, patching in the
// exception number, and routes exceptions the debugger claims to it.
static void OSExceptionInit(void) {
    __OSException exception;
    void* destAddr;
    u32* opCodeAddr;
    u32 oldOpCode;
    u8* handlerStart;
    u32 handlerSize;

    opCodeAddr = (u32*)__OSEVSetNumber;
    oldOpCode = *opCodeAddr;
    handlerStart = (u8*)__OSEVStart;
    handlerSize = (u32)((u8*)__OSEVEnd - (u8*)__OSEVStart);

    destAddr = (void*)(0x80000000 + OS_DBJUMPPOINT_ADDR);
    if (*(u32*)destAddr == 0) {
        DBPrintf("Installing OSDBIntegrator\n");
        memcpy(destAddr, (void*)__OSDBINTSTART, (u32)__OSDBJUMPSTART - (u32)__OSDBINTSTART);
        DCFlushRangeNoSync(destAddr, (u32)__OSDBJUMPSTART - (u32)__OSDBINTSTART);
        __sync();
        ICInvalidateRange(destAddr, (u32)__OSDBJUMPSTART - (u32)__OSDBINTSTART);
    }

    for (exception = 0; exception < __OS_EXCEPTION_MAX; exception++) {
        if (BI2DebugFlag && *BI2DebugFlag >= 2 && __DBIsExceptionMarked(exception)) {
            DBPrintf(">>> OSINIT: exception %d commandeered by TRK\n", exception);
            continue;
        }

        *opCodeAddr = oldOpCode | exception;

        if (__DBIsExceptionMarked(exception)) {
            DBPrintf(">>> OSINIT: exception %d vectored to debugger\n", exception);
            memcpy((void*)__DBVECTOR, (void*)__OSDBJUMPSTART, (u32)__OSDBJUMPEND - (u32)__OSDBJUMPSTART);
        } else {
            u32* ops = (u32*)__DBVECTOR;
            int cb;

            for (cb = 0; cb < (u32)__OSDBJUMPEND - (u32)__OSDBJUMPSTART; cb += sizeof(u32)) {
                *ops++ = 0x60000000;  // nop
            }
        }

        destAddr = (void*)(0x80000000 + __OSExceptionLocations[(u32)exception]);
        memcpy(destAddr, handlerStart, handlerSize);
        DCFlushRangeNoSync(destAddr, handlerSize);
        __sync();
        ICInvalidateRange(destAddr, handlerSize);
    }

    OSExceptionTable = (__OSExceptionHandler*)(0x80000000 + OS_EXCEPTIONTABLE_ADDR);
    for (exception = 0; exception < __OS_EXCEPTION_MAX; exception++) {
        __OSSetExceptionHandler(exception, (__OSExceptionHandler)OSDefaultExceptionHandler);
    }

    *opCodeAddr = oldOpCode;

    DBPrintf("Exceptions initialized...\n");
}

// Copied to 0x60: hands an exception to the debugger stub.
static asm void __OSDBIntegrator(void) {
    nofralloc
entry __OSDBINTSTART
    li r5, 0x40
    mflr r3
    stw r3, 0xc(r5)
    lwz r3, 0x8(r5)
    oris r3, r3, 0x8000
    mtlr r3
    li r3, 0x30
    mtmsr r3
    blr
}

// Copied over __DBVECTOR in the exception template when the debugger owns
// an exception.
static asm void __OSDBJump(void) {
    nofralloc
entry __OSDBJUMPSTART
    bla 0x60
entry __OSDBJUMPEND
}

__OSExceptionHandler __OSSetExceptionHandler(__OSException exception, __OSExceptionHandler handler) {
    __OSExceptionHandler oldHandler;

    oldHandler = OSExceptionTable[exception];
    OSExceptionTable[exception] = handler;
    return oldHandler;
}

__OSExceptionHandler __OSGetExceptionHandler(__OSException exception) {
    return OSExceptionTable[exception];
}

// Template copied to each exception vector. Saves r3-r5 and the special
// registers into the current context, then jumps to the handler from
// OSExceptionTable (li r3 at __OSEVSetNumber is patched with the number).
static asm void OSExceptionVector(void) {
    nofralloc
entry __OSEVStart
    mtspr 272, r4
    lwz r4, 0xc0(r0)
    stw r3, 0xc(r4)
    mfspr r3, 272
    stw r3, 0x10(r4)
    stw r5, 0x14(r4)
    lhz r3, 0x1a2(r4)
    ori r3, r3, 0x2
    sth r3, 0x1a2(r4)
    mfcr r3
    stw r3, 0x80(r4)
    mflr r3
    stw r3, 0x84(r4)
    mfctr r3
    stw r3, 0x88(r4)
    mfxer r3
    stw r3, 0x8c(r4)
    mfsrr0 r3
    stw r3, 0x198(r4)
    mfsrr1 r3
    stw r3, 0x19c(r4)
    mr r5, r3
entry __DBVECTOR
    nop
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
entry __OSEVSetNumber
    li r3, 0x0
    lwz r4, 0xd4(r0)
    rlwinm. r5, r5, 0, 30, 30
    bne L_80110AA0
    lis r5, OSDefaultExceptionHandler@ha
    addi r5, r5, OSDefaultExceptionHandler@l
    mtsrr0 r5
    rfi
L_80110AA0:
    clrlslwi r5, r3, 24, 2
    lwz r5, 0x3000(r5)
    mtsrr0 r5
    rfi
entry __OSEVEnd
    nop
}

static asm void OSDefaultExceptionHandler(register __OSException exception, register OSContext* context) {
    nofralloc
    stw r0, 0x0(r4)
    stw r1, 0x4(r4)
    stw r2, 0x8(r4)
    stmw r6, 0x18(r4)
    mfspr r0, 913
    stw r0, 0x1a8(r4)
    mfspr r0, 914
    stw r0, 0x1ac(r4)
    mfspr r0, 915
    stw r0, 0x1b0(r4)
    mfspr r0, 916
    stw r0, 0x1b4(r4)
    mfspr r0, 917
    stw r0, 0x1b8(r4)
    mfspr r0, 918
    stw r0, 0x1bc(r4)
    mfspr r0, 919
    stw r0, 0x1c0(r4)
    mfdsisr r5
    mfdar r6
    b __OSUnhandledException
}

void __OSPSInit(void) {
    PPCMthid2(PPCMfhid2() | 0xA0000000);
    ICFlashInvalidate();
    __sync();
    asm {
        li r3, 0
        mtspr 912, r3  // GQR0
    }
}

u8 __OSGetDIConfig(void) {
    return (u8)(__DIRegs[9] & 0xFF);
}
