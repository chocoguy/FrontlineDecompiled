#include <dolphin/dvd_priv.h>

#define __PIRegs ((volatile u32*)0xCC003000)

// Queued drive commands for the read workaround (seek twice / delay before a read).
typedef struct DVDCommand {
    /* 0x00 */ s32 cmd;  // -1 end, 1 read, 2 seek
    /* 0x04 */ void* addr;
    /* 0x08 */ u32 length;
    /* 0x0C */ u32 offset;
    /* 0x10 */ DVDLowCallback callback;
} DVDCommand;

typedef struct DVDBuffer {
    /* 0x0 */ void* addr;
    /* 0x4 */ u32 length;
    /* 0x8 */ u32 offset;
} DVDBuffer;

static DVDCommand CommandList[3];
static OSAlarm AlarmForWA;
static OSAlarm AlarmForTimeout;
static OSAlarm AlarmForBreak;
static DVDBuffer Prev;
static DVDBuffer Curr;

static BOOL FirstRead = TRUE;
static volatile BOOL StopAtNextInt;
static u32 LastLength;
static DVDLowCallback Callback;
static DVDLowCallback ResetCoverCallback;
static volatile OSTime LastResetEnd;
static volatile u32 ResetOccurred;
static volatile BOOL WaitingCoverClose;
static volatile BOOL Breaking;
static volatile u32 WorkAroundType;
static u32 WorkAroundSeekLocation;
static volatile OSTime LastReadFinished;
static OSTime LastReadIssued;
static volatile BOOL LastCommandWasRead;
static volatile u32 NextCommandNumber;

static void Read(void* addr, u32 length, u32 offset, DVDLowCallback callback);

void __DVDInitWA(void) {
    NextCommandNumber = 0;
    CommandList[0].cmd = -1;
    __DVDLowSetWAType(0, 0);
    OSInitAlarm();
}

static BOOL ProcessNextCommand(void) {
    s32 n = (s32)NextCommandNumber;

    if (CommandList[n].cmd == 1) {
        ++NextCommandNumber;
        Read(CommandList[n].addr, CommandList[n].length, CommandList[n].offset, CommandList[n].callback);
        return TRUE;
    } else if (CommandList[n].cmd == 2) {
        ++NextCommandNumber;
        DVDLowSeek(CommandList[n].offset, CommandList[n].callback);
        return TRUE;
    }
    return FALSE;
}

void __DVDInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    DVDLowCallback cb;
    OSContext exceptionContext;
    u32 cause = 0;
    u32 reg;
    u32 intr;
    u32 mask;

    OSCancelAlarm(&AlarmForTimeout);

    if (LastCommandWasRead) {
        LastReadFinished = __OSGetSystemTime();
        FirstRead = FALSE;
        Prev.addr = Curr.addr;
        Prev.length = Curr.length;
        Prev.offset = Curr.offset;
        if (StopAtNextInt == TRUE) {
            cause |= 8;
        }
    }

    LastCommandWasRead = FALSE;
    StopAtNextInt = FALSE;

    // Transfer complete (TC), device error (DE) and break (BRK), each with its mask bit.
    reg = __DIRegs[DI_STATUS];
    mask = reg & 0x2A;
    intr = (reg & 0x54) & (mask << 1);
    if (intr & 0x40) {
        cause |= 8;
    }
    if (intr & 0x10) {
        cause |= 1;
    }
    if (intr & 0x04) {
        cause |= 2;
    }
    if (cause) {
        ResetOccurred = FALSE;
    }
    __DIRegs[DI_STATUS] = intr | mask;

    if (ResetOccurred && (__OSGetSystemTime() - LastResetEnd) < OSMillisecondsToTicks(200)) {
        reg = __DIRegs[DI_COVER_STATUS];
        mask = reg & 2;
        intr = (reg & 4) & (mask << 1);
        if (intr & 4) {
            if (ResetCoverCallback) {
                ResetCoverCallback(4);
            }
            ResetCoverCallback = NULL;
        }
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
    } else if (WaitingCoverClose) {
        reg = __DIRegs[DI_COVER_STATUS];
        mask = reg & 2;
        intr = (reg & 4) & (mask << 1);
        if (intr & 4) {
            cause |= 4;
        }
        __DIRegs[DI_COVER_STATUS] = intr | mask;
        WaitingCoverClose = FALSE;
    } else {
        __DIRegs[DI_COVER_STATUS] = 0;
    }

    if ((cause & 8) && !Breaking) {
        cause &= ~8;
    }

    if (cause & 1) {
        if (ProcessNextCommand()) {
            return;
        }
    } else {
        CommandList[0].cmd = -1;
        NextCommandNumber = 0;
    }

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);

    if (cause) {
        cb = Callback;
        Callback = NULL;
        if (cb) {
            (*cb)(cause);
        }
        Breaking = FALSE;
    }

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void AlarmHandler(OSAlarm* alarm, OSContext* context) {
    ProcessNextCommand();
}

static void AlarmHandlerForTimeout(OSAlarm* alarm, OSContext* context) {
    OSContext tmpContext;
    DVDLowCallback callback;

    __OSMaskInterrupts(0x400);  // OS_INTERRUPTMASK_PI_DI
    OSClearContext(&tmpContext);
    OSSetCurrentContext(&tmpContext);
    callback = Callback;
    Callback = NULL;
    if (callback) {
        callback(0x10);  // timed out
    }
    OSClearContext(&tmpContext);
    OSSetCurrentContext(context);
}

static void SetTimeoutAlarm(OSTime timeout) {
    OSCreateAlarm(&AlarmForTimeout);
    OSSetAlarm(&AlarmForTimeout, timeout, AlarmHandlerForTimeout);
}

static void Read(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    LastCommandWasRead = TRUE;
    LastReadIssued = __OSGetSystemTime();

    __DIRegs[DI_CMDBUF0] = 0xA8000000;
    __DIRegs[DI_CMDBUF1] = offset / 4;
    __DIRegs[DI_CMDBUF2] = length;
    __DIRegs[DI_MAR] = (u32)addr;
    __DIRegs[DI_LENGTH] = length;
    LastLength = length;
    __DIRegs[DI_CR] = 3;

    if (length > 0xA00000) {
        SetTimeoutAlarm(OSSecondsToTicks((OSTime)20));
    } else {
        SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    }
}

// The drive's read-ahead cache covers this read: no seek workaround needed.
static BOOL HitCache(DVDBuffer* cur, DVDBuffer* prev) {
    u32 uNumBlocksPrev = (u32)(prev->offset + prev->length - 1) >> 15;
    u32 uNumBlocksCur = (u32)(cur->offset) >> 15;
    u32 cacheBlockSize = (u32)(DVDGetCurrentDiskID()->streaming ? TRUE : FALSE) ? 5 : 15;

    if ((uNumBlocksCur > uNumBlocksPrev - 2) || (uNumBlocksCur < uNumBlocksPrev + cacheBlockSize + 3)) {
        return TRUE;
    }
    return FALSE;
}

static void SeekTwiceBeforeRead(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    u32 newOffset;

    if ((offset & ~0x7FFF) == 0) {
        newOffset = 0;
    } else {
        newOffset = (offset & ~0x7FFF) + WorkAroundSeekLocation;
    }

    CommandList[0].cmd = 2;
    CommandList[0].offset = newOffset;
    CommandList[0].callback = callback;
    CommandList[1].cmd = 1;
    CommandList[1].addr = addr;
    CommandList[1].length = length;
    CommandList[1].offset = offset;
    CommandList[1].callback = callback;
    CommandList[2].cmd = -1;
    NextCommandNumber = 0;

    DVDLowSeek(newOffset, callback);
}

BOOL DVDLowRead(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    u32 prevLast;
    u32 currFirst;
    OSTime diff;

    __DIRegs[DI_LENGTH] = length;
    Curr.addr = addr;
    Curr.length = length;
    Curr.offset = offset;

    if (WorkAroundType == 0) {
        CommandList[0].cmd = -1;
        NextCommandNumber = 0;
        Read(addr, length, offset, callback);
    } else if (WorkAroundType == 1) {
        if (FirstRead) {
            SeekTwiceBeforeRead(addr, length, offset, callback);
        } else if (!HitCache(&Curr, &Prev)) {
            CommandList[0].cmd = -1;
            NextCommandNumber = 0;
            Read(addr, length, offset, callback);
        } else {
            prevLast = (u32)(Prev.offset + Prev.length - 1) >> 15;
            currFirst = (u32)Curr.offset >> 15;
            if (prevLast == currFirst || prevLast + 1 == currFirst) {
                diff = __OSGetSystemTime() - LastReadFinished;
                if (OSMillisecondsToTicks(5) < diff) {
                    CommandList[0].cmd = -1;
                    NextCommandNumber = 0;
                    Read(addr, length, offset, callback);
                } else {
                    CommandList[0].cmd = 1;
                    CommandList[0].addr = addr;
                    CommandList[0].length = length;
                    CommandList[0].offset = offset;
                    CommandList[0].callback = callback;
                    CommandList[1].cmd = -1;
                    NextCommandNumber = 0;
                    OSCreateAlarm(&AlarmForWA);
                    OSSetAlarm(&AlarmForWA, OSMillisecondsToTicks(5) - diff + OSMicrosecondsToTicks(500),
                               AlarmHandler);
                }
            } else {
                SeekTwiceBeforeRead(addr, length, offset, callback);
            }
        }
    }
    return TRUE;
}

BOOL DVDLowSeek(u32 offset, DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = 0xAB000000;
    __DIRegs[DI_CMDBUF1] = offset / 4;
    __DIRegs[DI_CR] = 1;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowWaitCoverClose(DVDLowCallback callback) {
    Callback = callback;
    WaitingCoverClose = TRUE;
    StopAtNextInt = FALSE;
    __DIRegs[DI_COVER_STATUS] = 2;
    return TRUE;
}

BOOL DVDLowReadDiskID(DVDDiskID* diskID, DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = 0xA8000040;
    __DIRegs[DI_CMDBUF1] = 0;
    __DIRegs[DI_CMDBUF2] = sizeof(DVDDiskID);
    __DIRegs[DI_MAR] = (u32)diskID;
    __DIRegs[DI_LENGTH] = sizeof(DVDDiskID);
    __DIRegs[DI_CR] = 3;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowStopMotor(DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = 0xE3000000;
    __DIRegs[DI_CR] = 1;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowRequestError(DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = 0xE0000000;
    __DIRegs[DI_CR] = 1;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowInquiry(void* info, DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = 0x12000000;
    __DIRegs[DI_CMDBUF2] = 0x20;
    __DIRegs[DI_MAR] = (u32)info;
    __DIRegs[DI_LENGTH] = 0x20;
    __DIRegs[DI_CR] = 3;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowAudioStream(u32 subcmd, u32 length, u32 offset, DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = subcmd | 0xE1000000;
    __DIRegs[DI_CMDBUF1] = offset >> 2;
    __DIRegs[DI_CMDBUF2] = length;
    __DIRegs[DI_CR] = 1;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowRequestAudioStatus(u32 subcmd, DVDLowCallback callback) {
    Callback = callback;
    StopAtNextInt = FALSE;
    __DIRegs[DI_CMDBUF0] = subcmd | 0xE2000000;
    __DIRegs[DI_CR] = 1;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

BOOL DVDLowAudioBufferConfig(BOOL enable, u32 size, DVDLowCallback callback) {
    u32 bufSize;
    u32 trigger;

    Callback = callback;
    StopAtNextInt = FALSE;
    bufSize = size;
    trigger = enable ? 0x10000 : 0;
    __DIRegs[DI_CMDBUF0] = 0xE4000000 | trigger | bufSize;
    __DIRegs[DI_CR] = 1;
    SetTimeoutAlarm(OSSecondsToTicks((OSTime)10));
    return TRUE;
}

void DVDLowReset(void) {
    u32 reg;
    OSTime resetStart;

    __DIRegs[DI_COVER_STATUS] = 2;
    reg = __PIRegs[9];
    __PIRegs[9] = (reg & ~4) | 1;

    resetStart = __OSGetSystemTime();
    while ((__OSGetSystemTime() - resetStart) < OSMicrosecondsToTicks(12)) {
    }

    __PIRegs[9] = reg | 5;
    ResetOccurred = TRUE;
    LastResetEnd = __OSGetSystemTime();
}

BOOL DVDLowBreak(void) {
    StopAtNextInt = TRUE;
    Breaking = TRUE;
    return TRUE;
}

DVDLowCallback DVDLowClearCallback(void) {
    DVDLowCallback old;

    __DIRegs[DI_COVER_STATUS] = 0;
    old = Callback;
    Callback = NULL;
    return old;
}

void __DVDLowSetWAType(u32 type, u32 seekLoc) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    WorkAroundType = type;
    WorkAroundSeekLocation = seekLoc;
    OSRestoreInterrupts(enabled);
}
