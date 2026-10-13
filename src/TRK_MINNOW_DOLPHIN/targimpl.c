#include <TRK_MINNOW_DOLPHIN/trk.h>

// Target layer: register/memory access, stepping and the exception entry points.

#define MSR_DR 0x10
#define MSR_SE 0x400
#define MSR_FP 0x2000

#define PPC_PROGRAMERROR 0x700
#define PPC_TRACE 0xD00

// DSMessageStepOptions
#define kDSStepIntoCount 0x00
#define kDSStepIntoRange 0x01
#define kDSStepOverCount 0x10
#define kDSStepOverRange 0x11

// Support request commands passed in r3 by the targsupp.s traps.
#define kDSWriteFile 0xD0
#define kDSReadFile 0xD1
#define kDSOpenFile 0xD2
#define kDSCloseFile 0xD3
#define kDSPositionFile 0xD4

#define kDSIONoError 0
#define kDSIOError 1

#define kValidMemoryReadable 0
#define kValidMemoryWriteable 1

#define SPR_HID2 920
#define SPR_GQR0 912
#define SPR_FPECR 1022

// Instructions assembled at run time to reach registers that have no C access.
#define INSTR_NOP 0x60000000
#define INSTR_BLR 0x4E800020
#define INSTR_PSQ_ST(psr, offset, rDest, w, gqr) \
    (0xF0000000 | ((psr) << 21) | ((rDest) << 16) | ((w) << 15) | ((gqr) << 12) | (offset))
#define INSTR_PSQ_L(psr, offset, rSrc, w, gqr) \
    (0xE0000000 | ((psr) << 21) | ((rSrc) << 16) | ((w) << 15) | ((gqr) << 12) | (offset))
#define INSTR_STW(rS, offset, rA) (0x90000000 | ((rS) << 21) | ((rA) << 16) | (offset))
#define INSTR_LWZ(rD, offset, rA) (0x80000000 | ((rD) << 21) | ((rA) << 16) | (offset))
#define INSTR_STFD(fS, offset, rA) (0xD8000000 | ((fS) << 21) | ((rA) << 16) | (offset))
#define INSTR_LFD(fD, offset, rA) (0xC8000000 | ((fD) << 21) | ((rA) << 16) | (offset))
#define INSTR_MFSPR(rD, spr) \
    (0x7C0002A6 | ((rD) << 21) | (((spr) & 0xFE0) << 6) | (((spr) & 0x1F) << 16))
#define INSTR_MTSPR(spr, rS) \
    (0x7C0003A6 | ((rS) << 21) | (((spr) & 0xFE0) << 6) | (((spr) & 0x1F) << 16))

typedef struct memRange {
    /* 0x0 */ u8* start;
    /* 0x4 */ u8* end;
    /* 0x8 */ BOOL readable;
    /* 0xC */ BOOL writeable;
} memRange;

typedef struct StopInfo_PPC {
    /* 0x0 */ u32 PC;
    /* 0x4 */ u32 PCInstruction;
    /* 0x8 */ u16 exceptionID;
} StopInfo_PPC;

typedef struct TRKExceptionStatus {
    /* 0x0 */ StopInfo_PPC exceptionInfo;
    /* 0xC */ u8 inTRK;
    /* 0xD */ u8 exceptionDetected;
} TRKExceptionStatus;

typedef struct TRKStepStatus {
    /* 0x00 */ BOOL active;
    /* 0x04 */ u8 type;
    /* 0x08 */ u32 count;
    /* 0x0C */ u32 rangeStart;
    /* 0x10 */ u32 rangeEnd;
} TRKStepStatus;

typedef void (*TRKRegAccessFunc)(void* srcDest, void* scratch);

const memRange gTRKMemMap[1] = {{(u8*)0, (u8*)0xFFFFFFFF, TRUE, TRUE}};

u8 gTRKRestoreFlags[9] = {0};
static TRKExceptionStatus gTRKExceptionStatus = {{0, 0, 0}, TRUE, FALSE};
static TRKStepStatus gTRKStepStatus = {FALSE, kDSStepIntoCount, 0, 0, 0};

static u16 TRK_saved_exceptionID;
TRKState_PPC gTRKState;
ProcessorState_PPC gTRKCPUState;
u32 gTRKSaveState[0x94 / 4];
u32 TRKvalue128_temp[4];

DSError TRKValidMemory32(const void* addr, u32 length, u8 readWriteable);
void TRKPostInterruptEvent(void);
DSError TRKPPCAccessFPRegister(void* srcDestPtr, u32 fpr, BOOL read);
void TRKInterruptHandlerEnableInterrupts(void);
void TRKExceptionHandler(u16 exceptionID);
void TRKInterruptHandler(u16 exceptionID);
void TRK_ppc_memcpy(void* dst, const void* src, int n, u32 dstMSR, u32 srcMSR);
void TRKUARTInterruptHandler(void);

#pragma auto_inline off
DSError TRKValidMemory32(const void* addr, u32 length, u8 readWriteable) {
    DSError err = kInvalidMemory;
    const u8* start;
    const u8* end;
    int i;

    start = (const u8*)addr;
    end = (const u8*)addr + (length - 1);

    if (end < start) {
        return kInvalidMemory;
    }

    for (i = 0; i < (int)(sizeof(gTRKMemMap) / sizeof(memRange)); i++) {
        if (start <= gTRKMemMap[i].end && end >= gTRKMemMap[i].start) {
            if ((readWriteable == kValidMemoryReadable && !gTRKMemMap[i].readable) ||
                (readWriteable == kValidMemoryWriteable && !gTRKMemMap[i].writeable)) {
                err = kInvalidMemory;
            } else {
                err = kNoError;
                if (start < gTRKMemMap[i].start) {
                    err = TRKValidMemory32(start, gTRKMemMap[i].start - start, readWriteable);
                }
                if (err == kNoError && end > gTRKMemMap[i].end) {
                    err = TRKValidMemory32(gTRKMemMap[i].end, end - gTRKMemMap[i].end, readWriteable);
                }
            }
            break;
        }
    }

    return err;
}

DSError TRKTargetAccessMemory(void* data, u32 start, u32* length, int memorySpace, BOOL read) {
    DSError error;
    u32 target_msr;
    void* addr;
    u32 trk_msr;
    TRKExceptionStatus tempExceptionStatus = gTRKExceptionStatus;

    gTRKExceptionStatus.exceptionDetected = FALSE;

    addr = (void*)TRKTargetTranslate(start);
    error = TRKValidMemory32(addr, *length, read ? kValidMemoryReadable : kValidMemoryWriteable);

    if (error != kNoError) {
        *length = 0;
    } else {
        target_msr = __TRK_get_MSR();
        trk_msr = target_msr | (gTRKCPUState.Extended1.MSR & MSR_DR);

        if (read) {
            TRK_ppc_memcpy(data, addr, *length, target_msr, trk_msr);
        } else {
            TRK_ppc_memcpy(addr, data, *length, trk_msr, target_msr);
            TRK_flush_cache(addr, *length);
            if ((void*)start != addr) {
                TRK_flush_cache((void*)start, *length);
            }
        }
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *length = 0;
        error = kCWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}
#pragma auto_inline reset

DSError TRKTargetAccessDefault(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize,
                               BOOL read) {
    DSError error;
    u32 count;
    u32* data;
    TRKExceptionStatus tempExceptionStatus;

    if (lastRegister > 0x24) {
        return kInvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    data = gTRKCPUState.Default.GPR + firstRegister;
    count = (lastRegister - firstRegister) + 1;
    *registerStorageSize = count * sizeof(u32);

    if (read) {
        error = TRKAppendBuffer_ui32(b, data, count);
    } else {
        error = TRKReadBuffer_ui32(b, data, count);
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registerStorageSize = 0;
        error = kCWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}

DSError TRKTargetAccessFP(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize, BOOL read) {
    u64 temp;
    DSError error;
    TRKExceptionStatus tempExceptionStatus;
    u32 current;

    if (lastRegister > 0x21) {
        return kInvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    __TRK_set_MSR(__TRK_get_MSR() | MSR_FP);

    *registerStorageSize = 0;
    error = kNoError;

    for (current = firstRegister; current <= lastRegister && error == kNoError;
         current++, *registerStorageSize += sizeof(u64)) {
        if (read) {
            TRKPPCAccessFPRegister(&temp, current, read);
            error = TRKAppendBuffer1_ui64(b, temp);
        } else {
            TRKReadBuffer1_ui64(b, &temp);
            error = TRKPPCAccessFPRegister(&temp, current, read);
        }
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registerStorageSize = 0;
        error = kCWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}

DSError TRKTargetAccessExtended1(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize,
                                 BOOL read) {
    TRKExceptionStatus tempExceptionStatus;
    DSError error;
    u32* data;
    int count;

    if (lastRegister > 0x60) {
        return kInvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    *registerStorageSize = 0;

    if (firstRegister <= lastRegister) {
        data = (u32*)&gTRKCPUState.Extended1 + firstRegister;
        count = (lastRegister - firstRegister) + 1;
        *registerStorageSize += count * sizeof(u32);

        if (read) {
            error = TRKAppendBuffer_ui32(b, data, count);
        } else {
            if (data <= &gTRKCPUState.Extended1.TBU && (data + count - 1) >= &gTRKCPUState.Extended1.TBL) {
                gTRKRestoreFlags[0] = TRUE;
            }
            if (data <= &gTRKCPUState.Extended1.DEC && (data + count - 1) >= &gTRKCPUState.Extended1.DEC) {
                gTRKRestoreFlags[1] = TRUE;
            }
            error = TRKReadBuffer_ui32(b, data, count);
        }
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registerStorageSize = 0;
        error = kCWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}

// Patches the trailing blr into a 10-instruction stub and calls it with
// r3 = value, r4 = scratch.
static inline DSError TRKPPCAccessSpecialReg(void* value, u32* access_func, BOOL read) {
    TRKRegAccessFunc asm_access;

    access_func[9] = INSTR_BLR;
    TRK_flush_cache(access_func, 10 * sizeof(u32));
    asm_access = (TRKRegAccessFunc)access_func;
    asm_access(value, TRKvalue128_temp);
    return kNoError;
}

static inline DSError TRKPPCAccessSPR(void* value, u32 spr_register_num, BOOL read) {
    u32 access_func[10] = {INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP,
                           INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP};

    if (read) {
        access_func[0] = INSTR_MFSPR(4, spr_register_num);
        access_func[1] = INSTR_STW(4, 0, 3);
    } else {
        access_func[0] = INSTR_LWZ(4, 0, 3);
        access_func[1] = INSTR_MTSPR(spr_register_num, 4);
    }

    return TRKPPCAccessSpecialReg(value, access_func, read);
}

static inline DSError TRKPPCAccessPairedSingleRegister(void* srcDestPtr, u32 psr, BOOL read) {
    u32 instructionData[10] = {INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP,
                               INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP};

    instructionData[0] = INSTR_PSQ_L(psr, 0, 3, 0, 0);
    if (read) {
        instructionData[0] = INSTR_PSQ_ST(psr, 0, 3, 0, 0);
    }

    return TRKPPCAccessSpecialReg(srcDestPtr, instructionData, read);
}

DSError TRKTargetAccessExtended2(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize,
                                 BOOL read) {
    TRKExceptionStatus savedException;
    u32 i;
    u32 value_buf0[1];
    u64 value_buf;
    DSError err;

    if (lastRegister > 0x1F) {
        return kInvalidRegister;
    }

    savedException = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    // Enable paired singles and quantized load/store, with an identity GQR0.
    TRKPPCAccessSPR(value_buf0, SPR_HID2, TRUE);
    value_buf0[0] |= 0xA0000000;
    TRKPPCAccessSPR(value_buf0, SPR_HID2, FALSE);
    value_buf0[0] = 0;
    TRKPPCAccessSPR(value_buf0, SPR_GQR0, FALSE);

    *registerStorageSize = 0;
    err = kNoError;

    for (i = firstRegister; i <= lastRegister && err == kNoError; i++) {
        if (read) {
            TRKPPCAccessPairedSingleRegister(&value_buf, i, read);
            err = TRKAppendBuffer1_ui64(b, value_buf);
        } else {
            TRKReadBuffer1_ui64(b, &value_buf);
            err = TRKPPCAccessPairedSingleRegister(&value_buf, i, read);
        }
        *registerStorageSize += sizeof(u64);
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registerStorageSize = 0;
        err = kCWDSException;
    }

    gTRKExceptionStatus = savedException;
    return err;
}

DSError TRKTargetVersions(DSVersions* versions) {
    versions->kernelMajor = 0;
    versions->kernelMinor = 10;
    versions->protocolMajor = 1;
    versions->protocolMinor = 10;
    return kNoError;
}

DSError TRKTargetSupportMask(u8 mask[32]) {
    mask[0] = 0x7A;
    mask[1] = 0;
    mask[2] = 0x4F;
    mask[3] = 7;
    mask[4] = 0;
    mask[5] = 0;
    mask[6] = 0;
    mask[7] = 0;
    mask[8] = 0;
    mask[9] = 0;
    mask[10] = 0;
    mask[11] = 0;
    mask[12] = 0;
    mask[13] = 0;
    mask[14] = 0;
    mask[15] = 0;
    mask[16] = 1;
    mask[17] = 0;
    mask[18] = 3;
    mask[19] = 0;
    mask[20] = 0;
    mask[21] = 0;
    mask[22] = 0;
    mask[23] = 0;
    mask[24] = 0;
    mask[25] = 0;
    mask[26] = 3;
    mask[27] = 0;
    mask[28] = 0;
    mask[29] = 0;
    mask[30] = 0;
    mask[31] = 0x80;
    return kNoError;
}

DSError TRKTargetCPUType(DSCPUType* cpuType) {
    cpuType->cpuMajor = 0;
    cpuType->cpuMinor = TRKTargetCPUMinorType();
    cpuType->bigEndian = gTRKBigEndian;
    cpuType->defaultTypeSize = 4;
    cpuType->fpTypeSize = 8;
    cpuType->extended1TypeSize = 4;
    cpuType->extended2TypeSize = 8;
    return kNoError;
}

static inline DSError TRKTargetReadInstruction(void* data, u32 start) {
    DSError error;
    u32 registersLength = 4;

    error = TRKTargetAccessMemory(data, start, &registersLength, kUserMemory, TRUE);
    if (error == kNoError && registersLength != 4) {
        error = kInvalidMemory;
    }
    return error;
}

void TRKPostInterruptEvent(void) {
    NubEventID eventType;
    u32 inst;
    TRKEvent event;

    if (gTRKState.inputActivated) {
        gTRKState.inputActivated = FALSE;
    } else {
        switch ((u16)gTRKCPUState.Extended1.exceptionID) {
        case PPC_PROGRAMERROR:
        case PPC_TRACE:
            TRKTargetReadInstruction(&inst, gTRKCPUState.Default.PC);
            // twui r0, 0x0 is the support-request trap.
            if (inst == 0x0FE00000) {
                eventType = kSupportEvent;
            } else {
                eventType = kBreakpointEvent;
            }
            break;
        default:
            eventType = kExceptionEvent;
            break;
        }

        TRKConstructEvent(&event, eventType);
        TRKPostEvent(&event);
    }
}

static inline BOOL TRKTargetStepDone(void) {
    BOOL result = TRUE;

    if (gTRKStepStatus.active && (u16)gTRKCPUState.Extended1.exceptionID == PPC_TRACE) {
        switch (gTRKStepStatus.type) {
        case kDSStepIntoCount:
            if (gTRKStepStatus.count > 0) {
                result = FALSE;
            }
            break;
        case kDSStepIntoRange:
            if (gTRKCPUState.Default.PC >= gTRKStepStatus.rangeStart &&
                gTRKCPUState.Default.PC <= gTRKStepStatus.rangeEnd) {
                result = FALSE;
            }
            break;
        default:
            break;
        }
    }

    return result;
}

static inline DSError TRKTargetDoStep(void) {
    gTRKStepStatus.active = TRUE;
    gTRKCPUState.Extended1.MSR |= MSR_SE;

    if (gTRKStepStatus.type == kDSStepIntoCount || gTRKStepStatus.type == kDSStepOverCount) {
        gTRKStepStatus.count--;
    }

    TRKTargetSetStopped(FALSE);
    return kNoError;
}

static inline BOOL TRKTargetCheckStep(void) {
    if (gTRKStepStatus.active) {
        gTRKCPUState.Extended1.MSR &= ~MSR_SE;

        if (TRKTargetStepDone()) {
            gTRKStepStatus.active = FALSE;
        } else {
            TRKTargetDoStep();
        }
    }

    return gTRKStepStatus.active;
}

DSError TRKTargetInterrupt(TRKEvent* event) {
    DSError error = kNoError;

    switch (event->eventType) {
    case kBreakpointEvent:
    case kExceptionEvent:
        if (TRKTargetCheckStep() == FALSE) {
            TRKTargetSetStopped(TRUE);
            error = TRKDoNotifyStopped(kDSNotifyStopped);
        }
        break;
    default:
        break;
    }

    return error;
}

void TRKTargetAddStopInfo(TRKBuffer* buffer) {
    DSError error;
    u32 instruction;
    int i;

    error = TRKAppendBuffer1_ui32(buffer, gTRKCPUState.Default.PC);
    if (error == kNoError) {
        error = TRKTargetReadInstruction(&instruction, gTRKCPUState.Default.PC);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui32(buffer, instruction);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui16(buffer, gTRKCPUState.Extended1.exceptionID);
    }
    if (error == kNoError) {
        // The original truncates every register to 16 bits here.
        for (i = 0; i < 32; i++) {
            TRKAppendBuffer1_ui32(buffer, (u16)gTRKCPUState.Default.GPR[i]);
        }
        for (i = 0; i < 32; i++) {
            TRKAppendBuffer1_ui64(buffer, (u16)gTRKCPUState.Float.FPR[i]);
        }
    }
}

void TRKTargetAddExceptionInfo(TRKBuffer* buffer) {
    DSError error;
    u32 instruction;

    error = TRKAppendBuffer1_ui32(buffer, gTRKExceptionStatus.exceptionInfo.PC);
    if (error == kNoError) {
        error = TRKTargetReadInstruction(&instruction, gTRKExceptionStatus.exceptionInfo.PC);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui32(buffer, instruction);
    }
    if (error == kNoError) {
        TRKAppendBuffer1_ui16(buffer, gTRKExceptionStatus.exceptionInfo.exceptionID);
    }
}

DSError TRKTargetSingleStep(u32 count, BOOL stepOver) {
    DSError error = kNoError;

    if (stepOver) {
        error = kUnsupportedError;
    } else {
        gTRKStepStatus.type = kDSStepIntoCount;
        gTRKStepStatus.count = count;
        error = TRKTargetDoStep();
    }

    return error;
}

DSError TRKTargetStepOutOfRange(u32 rangeStart, u32 rangeEnd, BOOL stepOver) {
    DSError error = kNoError;

    if (stepOver) {
        error = kUnsupportedError;
    } else {
        gTRKStepStatus.type = kDSStepIntoRange;
        gTRKStepStatus.rangeStart = rangeStart;
        gTRKStepStatus.rangeEnd = rangeEnd;
        error = TRKTargetDoStep();
    }

    return error;
}

u32 TRKTargetGetPC(void) {
    return gTRKCPUState.Default.PC;
}

DSError TRKTargetSupportRequest(void) {
    DSError error;
    u32 position;
    u32* length;
    u8 commandId;
    TRKEvent event;
    u8 ioResult;

    commandId = gTRKCPUState.Default.GPR[3];
    if (commandId != kDSReadFile && commandId != kDSWriteFile && commandId != kDSOpenFile &&
        commandId != kDSCloseFile && commandId != kDSPositionFile) {
        TRKConstructEvent(&event, kExceptionEvent);
        TRKPostEvent(&event);
        return kNoError;
    }

    if (commandId == kDSOpenFile) {
        error = HandleOpenFileSupportRequest((const char*)gTRKCPUState.Default.GPR[4], gTRKCPUState.Default.GPR[5],
                                             (u32*)gTRKCPUState.Default.GPR[6], &ioResult);
        if (ioResult == kDSIONoError && error != kNoError) {
            ioResult = kDSIOError;
        }
        gTRKCPUState.Default.GPR[3] = ioResult;
    } else if (commandId == kDSCloseFile) {
        error = HandleCloseFileSupportRequest(gTRKCPUState.Default.GPR[4], &ioResult);
        if (ioResult == kDSIONoError && error != kNoError) {
            ioResult = kDSIOError;
        }
        gTRKCPUState.Default.GPR[3] = ioResult;
    } else if (commandId == kDSPositionFile) {
        position = *(u32*)gTRKCPUState.Default.GPR[5];
        error = HandlePositionFileSupportRequest(gTRKCPUState.Default.GPR[4], &position, gTRKCPUState.Default.GPR[6],
                                                 &ioResult);
        if (ioResult == kDSIONoError && error != kNoError) {
            ioResult = kDSIOError;
        }
        gTRKCPUState.Default.GPR[3] = ioResult;
        *(u32*)gTRKCPUState.Default.GPR[5] = position;
    } else {
        length = (u32*)gTRKCPUState.Default.GPR[5];
        error = TRKSuppAccessFile((u8)gTRKCPUState.Default.GPR[4], (u8*)gTRKCPUState.Default.GPR[6], length,
                                  &ioResult, TRUE, commandId == kDSReadFile);
        if (ioResult == kDSIONoError && error != kNoError) {
            ioResult = kDSIOError;
        }
        gTRKCPUState.Default.GPR[3] = ioResult;
        if (commandId == kDSReadFile) {
            TRK_flush_cache((void*)gTRKCPUState.Default.GPR[6], *length);
        }
    }

    gTRKCPUState.Default.PC += 4;
    return error;
}

DSError TRKTargetFlushCache(u8 options, void* start, u32 end) {
    if ((u32)start < end) {
        TRK_flush_cache(start, end - (u32)start);
        return kNoError;
    }
    return kInvalidMemory;
}

BOOL TRKTargetStopped(void) {
    return gTRKState.isStopped;
}

void TRKTargetSetStopped(unsigned int stopped) {
    gTRKState.isStopped = stopped;
}

DSError TRKTargetStop(void) {
    TRKTargetSetStopped(TRUE);
    return kNoError;
}

#pragma auto_inline off
DSError TRKPPCAccessFPRegister(void* srcDestPtr, u32 fpr, BOOL read) {
    DSError error = kNoError;
    u32 instructionData[10] = {INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP,
                               INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP};

    if (fpr < 0x20) {
        instructionData[0] = INSTR_LFD(fpr, 0, 3);
        if (read) {
            instructionData[0] = INSTR_STFD(fpr, 0, 3);
        }
        error = TRKPPCAccessSpecialReg(srcDestPtr, instructionData, read);
    } else if (fpr == 0x20) {
        // FPSCR: only the low word is meaningful.
        *(u64*)srcDestPtr &= 0xFFFFFFFF;
    } else if (fpr == 0x21) {
        // FPECR
        if (!read) {
            *(u32*)srcDestPtr = *((u32*)srcDestPtr + 1);
        }
        error = TRKPPCAccessSPR(srcDestPtr, SPR_FPECR, read);
        if (read) {
            *(u64*)srcDestPtr = *(u32*)srcDestPtr;
        }
    }

    return error;
}
#pragma auto_inline reset

void TRKTargetSetInputPendingPtr(void* ptr) {
    gTRKState.inputPendingPtr = ptr;
}

// Restores the monitor context saved by TRKSwapAndGo and reports the
// interrupt to the nub.
asm void TRKInterruptHandlerEnableInterrupts(void) {
    nofralloc
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r0, 0x8c(r2)
    sync
    mtmsr r0
    sync
    lwz r0, 0x80(r2)
    mtlr r0
    lwz r0, 0x84(r2)
    mtctr r0
    lwz r0, 0x88(r2)
    mtxer r0
    lwz r0, 0x94(r2)
    mtspr 18, r0
    lwz r0, 0x90(r2)
    mtspr 19, r0
    lmw r3, 0xc(r2)
    lwz r0, 0x0(r2)
    lwz r1, 0x4(r2)
    lwz r2, 0x8(r2)
    b TRKPostInterruptEvent
}

// Saves the monitor context and resumes the target. If input arrived while
// the monitor was running, stays in the monitor instead.
asm void TRKSwapAndGo(void) {
    nofralloc
    lis r3, gTRKState@h
    ori r3, r3, gTRKState@l
    stmw r0, 0x0(r3)
    mfmsr r0
    stw r0, 0x8c(r3)
    mflr r0
    stw r0, 0x80(r3)
    mfctr r0
    stw r0, 0x84(r3)
    mfxer r0
    stw r0, 0x88(r3)
    mfspr r0, 18
    stw r0, 0x94(r3)
    mfspr r0, 19
    stw r0, 0x90(r3)
    li r1, -0x7ffe
    nor r1, r1, r1
    mfmsr r3
    and r3, r3, r1
    mtmsr r3
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, 0xa0(r2)
    lbz r2, 0x0(r2)
    cmpwi r2, 0x0
    beq L_80135AC4
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    li r3, 0x1
    stb r3, 0x9c(r2)
    b TRKInterruptHandlerEnableInterrupts
L_80135AC4:
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 0x0
    stb r3, 0xc(r2)
    bl TRKRestoreExtended1Block
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    lmw r27, 0x80(r2)
    mtspr 26, r27
    mtlr r28
    mtcrf 255, r29
    mtctr r30
    mtxer r31
    lmw r3, 0xc(r2)
    lwz r0, 0x0(r2)
    lwz r1, 0x4(r2)
    lwz r2, 0x8(r2)
    rfi
}

// An exception taken while the monitor itself touches target memory or
// registers: records it, skips the faulting instruction and returns.
asm void TRKExceptionHandler(u16 exceptionID) {
    nofralloc
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    sth r3, 0x8(r2)
    mfspr r3, 26
    stw r3, 0x0(r2)
    lhz r3, 0x8(r2)
    cmpwi r3, 0x200
    beq L_80135A18
    cmpwi r3, 0x300
    beq L_80135A18
    cmpwi r3, 0x400
    beq L_80135A18
    cmpwi r3, 0x600
    beq L_80135A18
    cmpwi r3, 0x700
    beq L_80135A18
    cmpwi r3, 0x800
    beq L_80135A18
    cmpwi r3, 0x1000
    beq L_80135A18
    cmpwi r3, 0x1100
    beq L_80135A18
    cmpwi r3, 0x1200
    beq L_80135A18
    cmpwi r3, 0x1300
    beq L_80135A18
    b L_80135A24
L_80135A18:
    mfspr r3, 26
    addi r3, r3, 0x4
    mtspr 26, r3
L_80135A24:
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 0x1
    stb r3, 0xd(r2)
    mfspr r3, 275
    mtcrf 255, r3
    mfspr r2, 273
    mfspr r3, 274
    rfi
}

// Common entry from the hooked exception vectors (r3 = vector offset).
asm void TRKInterruptHandler(u16 exceptionID) {
    nofralloc
    mtspr 26, r2
    mtspr 27, r4
    mfspr r4, 275
    mfcr r2
    mtspr 275, r2
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, 0x8c(r2)
    ori r2, r2, 0x8002
    xori r2, r2, 0x8002
    sync
    mtmsr r2
    sync
    lis r2, TRK_saved_exceptionID@h
    ori r2, r2, TRK_saved_exceptionID@l
    sth r3, 0x0(r2)
    cmpwi r3, 0x500
    bne L_801358E0
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    mflr r3
    stw r3, 0x42c(r2)
    bl TRKUARTInterruptHandler
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    lwz r3, 0x42c(r2)
    mtlr r3
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, 0xa0(r2)
    lbz r2, 0x0(r2)
    cmpwi r2, 0x0
    beq L_801358C4
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    lbz r2, 0xc(r2)
    cmpwi r2, 0x1
    beq L_801358C4
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    li r3, 0x1
    stb r3, 0x9c(r2)
    b L_801358E0
L_801358C4:
    lis r2, gTRKSaveState@h
    ori r2, r2, gTRKSaveState@l
    lwz r3, 0x88(r2)
    mtcrf 255, r3
    lwz r3, 0xc(r2)
    lwz r2, 0x8(r2)
    rfi
L_801358E0:
    lis r2, TRK_saved_exceptionID@h
    ori r2, r2, TRK_saved_exceptionID@l
    lhz r3, 0x0(r2)
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    lbz r2, 0xc(r2)
    cmpwi r2, 0x0
    bne TRKExceptionHandler
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    stw r0, 0x0(r2)
    stw r1, 0x4(r2)
    mfspr r0, 273
    stw r0, 0x8(r2)
    sth r3, 0x2f8(r2)
    sth r3, 0x2fa(r2)
    mfspr r0, 274
    stw r0, 0xc(r2)
    stmw r4, 0x10(r2)
    mfspr r27, 26
    mflr r28
    mfspr r29, 275
    mfctr r30
    mfxer r31
    stmw r27, 0x80(r2)
    bl TRKSaveExtended1Block
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 0x1
    stb r3, 0xc(r2)
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r0, 0x8c(r2)
    sync
    mtmsr r0
    sync
    lwz r0, 0x80(r2)
    mtlr r0
    lwz r0, 0x84(r2)
    mtctr r0
    lwz r0, 0x88(r2)
    mtxer r0
    lwz r0, 0x94(r2)
    mtspr 18, r0
    lwz r0, 0x90(r2)
    mtspr 19, r0
    lmw r3, 0xc(r2)
    lwz r0, 0x0(r2)
    lwz r1, 0x4(r2)
    lwz r2, 0x8(r2)
    b TRKPostInterruptEvent
}

// Byte copy with the MSR switched per access, so source and destination can
// be read/written with different address translation.
asm void TRK_ppc_memcpy(void* dst, const void* src, int n, u32 dstMSR, u32 srcMSR) {
    nofralloc
    mfmsr r8
    li r10, 0x0
L_801357E4:
    cmpw r10, r5
    beq L_8013580C
    mtmsr r7
    sync
    lbzx r9, r10, r4
    mtmsr r6
    sync
    stbx r9, r10, r3
    addi r10, r10, 0x1
    b L_801357E4
L_8013580C:
    mtmsr r8
    sync
    blr
}

asm void __TRK_set_MSR(u32 msr) {
    nofralloc
    mtmsr r3
    blr
}

asm u32 __TRK_get_MSR(void) {
    nofralloc
    mfmsr r3
    blr
}
