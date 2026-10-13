#include <dolphin/dvd_priv.h>
#include <string.h>

// Command block states.
#define DVD_STATE_FATAL_ERROR -1
#define DVD_STATE_END 0
#define DVD_STATE_BUSY 1
#define DVD_STATE_WAITING 2
#define DVD_STATE_COVER_CLOSED 3
#define DVD_STATE_NO_DISK 4
#define DVD_STATE_COVER_OPEN 5
#define DVD_STATE_WRONG_DISK 6
#define DVD_STATE_MOTOR_STOPPED 7
#define DVD_STATE_PAUSING 8
#define DVD_STATE_IGNORED 9
#define DVD_STATE_CANCELED 10
#define DVD_STATE_RETRY 11

#define DVD_RESULT_FATAL_ERROR -1
#define DVD_RESULT_IGNORED -2
#define DVD_RESULT_CANCELED -3
#define DVD_RESULT_COVER_CLOSED -4

// Commands.
#define DVD_COMMAND_READ 1
#define DVD_COMMAND_SEEK 2
#define DVD_COMMAND_CHANGE_DISK 3
#define DVD_COMMAND_BSREAD 4
#define DVD_COMMAND_READID 5
#define DVD_COMMAND_INITSTREAM 6
#define DVD_COMMAND_CANCELSTREAM 7
#define DVD_COMMAND_STOP_STREAM_AT_END 8
#define DVD_COMMAND_REQUEST_AUDIO_ERROR 9
#define DVD_COMMAND_REQUEST_PLAY_ADDR 10
#define DVD_COMMAND_REQUEST_START_ADDR 11
#define DVD_COMMAND_REQUEST_LENGTH 12
#define DVD_COMMAND_AUDIO_BUFFER_CONFIG 13
#define DVD_COMMAND_INQUIRY 14
#define DVD_COMMAND_BS_CHANGE_DISK 15

// Low memory boot info (as far as the DVD library needs it).
typedef struct OSBootInfoDVD {
    /* 0x00 */ DVDDiskID DVDDiskID;
    /* 0x20 */ u32 magic;
    /* 0x24 */ u8 pad[0x14];
    /* 0x38 */ void* FSTLocation;
    /* 0x3C */ u32 FSTMaxLength;
} OSBootInfoDVD;

typedef void (*stateFunc)(DVDCommandBlock* block);

static u8 tmpBuffer[0x80] __attribute__((aligned(32)));
static DVDCommandBlock DummyCommandBlock;
static OSAlarm ResetAlarm;

static BOOL autoInvalidation = TRUE;

static DVDCommandBlock* executing;
static DVDDiskID* currID;
static OSBootInfoDVD* bootInfo;
static volatile BOOL PauseFlag;
static volatile BOOL PausingFlag;
static volatile BOOL AutoFinishing;
static volatile BOOL FatalErrorFlag;
static volatile u32 CurrCommand;
static volatile u32 Canceling;
static DVDCBCallback CancelCallback;
static volatile u32 ResumeFromHere;
static volatile u32 CancelLastError;
static volatile u32 LastError;
static volatile s32 NumInternalRetry;
static volatile BOOL ResetRequired;
static volatile BOOL FirstTimeInBootrom;
static BOOL DVDInitialized;
stateFunc LastState;

static void stateReadingFST(void);
static void cbForStateReadingFST(u32 intType);
static void cbForStateError(u32 intType);
static void stateTimeout(void);
static void stateGettingError(void);
static u32 CategorizeError(u32 error);
static BOOL CheckCancel(u32 resume);
static void cbForStateGettingError(u32 intType);
static void cbForUnrecoveredError(u32 intType);
static void cbForUnrecoveredErrorRetry(u32 intType);
static void stateGoToRetry(void);
static void cbForStateGoToRetry(u32 intType);
static void stateCheckID(void);
static void stateCheckID3(DVDCommandBlock* block);
static void stateCheckID2(DVDCommandBlock* block);
static void cbForStateCheckID1(u32 intType);
static void cbForStateCheckID2(u32 intType);
static void cbForStateCheckID3(u32 intType);
static void stateCoverClosed(void);
static void stateCoverClosed_CMD(DVDCommandBlock* block);
static void cbForStateCoverClosed(u32 intType);
static void stateMotorStopped(void);
static void cbForStateMotorStopped(u32 intType);
static void stateReady(void);
static void stateBusy(DVDCommandBlock* block);
static void cbForStateBusy(u32 intType);

void DVDInit(void) {
    if (DVDInitialized) {
        return;
    }

    OSInitAlarm();
    DVDInitialized = TRUE;
    __DVDFSInit();
    __DVDClearWaitingQueue();
    __DVDInitWA();

    bootInfo = (OSBootInfoDVD*)0x80000000;
    currID = &(bootInfo->DVDDiskID);

    __OSSetInterruptHandler(21, __DVDInterruptHandler);  // __OS_INTERRUPT_PI_DI
    __OSUnmaskInterrupts(0x400);                        // OS_INTERRUPTMASK_PI_DI
    OSInitThreadQueue(&__DVDThreadQueue);

    __DIRegs[DI_STATUS] = 42;
    __DIRegs[DI_COVER_STATUS] = 0;

    if (bootInfo->magic == 0xE5207C22) {
        OSReport("app booted via JTAG\n");
        OSReport("load fst\n");
        __fstLoad();
    } else if (bootInfo->magic == 0x0D15EA5E) {
        OSReport("app booted from bootrom\n");
    } else {
        FirstTimeInBootrom = TRUE;
        OSReport("bootrom\n");
    }
}

static void stateReadingFST(void) {
    LastState = (stateFunc)stateReadingFST;
    DVDLowRead(bootInfo->FSTLocation, (((DVDBB2*)tmpBuffer)->FSTLength + 31) & ~31,
               ((DVDBB2*)tmpBuffer)->FSTPosition, cbForStateReadingFST);
}

static void cbForStateReadingFST(u32 intType) {
    DVDCommandBlock* finished;

    if (intType == 16) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        finished = executing;
        executing = &DummyCommandBlock;
        finished->state = DVD_STATE_END;
        if (finished->callback) {
            (finished->callback)(0, finished);
        }
        stateReady();
    } else {
        stateGettingError();
    }
}

static void cbForStateError(u32 intType) {
    DVDCommandBlock* finished;

    if (intType == 16) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    FatalErrorFlag = TRUE;
    finished = executing;
    executing = &DummyCommandBlock;
    if (finished->callback) {
        (finished->callback)(DVD_RESULT_FATAL_ERROR, finished);
    }

    if (Canceling) {
        Canceling = FALSE;
        if (CancelCallback) {
            (CancelCallback)(0, finished);
        }
    }

    stateReady();
}

static void stateTimeout(void) {
    __DVDStoreErrorCode(0x01234568);
    DVDReset();
    cbForStateError(0);
}

// The drive timed out (the low-level callback reports 16): fatal.
static inline void TimedOut(void) {
    executing->state = DVD_STATE_FATAL_ERROR;
    stateTimeout();
}

// The drive reported an error the library can't recover from: stop the motor and go fatal.
static inline void Unrecoverable(u32 code) {
    executing->state = DVD_STATE_FATAL_ERROR;
    __DVDStoreErrorCode(code);
    DVDLowStopMotor(cbForStateError);
}

static void stateGettingError(void) {
    DVDLowRequestError(cbForStateGettingError);
}

static u32 CategorizeError(u32 error) {
    if (error == 0x20400) {
        LastError = error;
        return 1;
    }

    error &= 0xFFFFFF;

    if ((error == 0x62800) || (error == 0x23A00) || (error == 0xB5A01)) {
        return 0;
    }

    ++NumInternalRetry;
    if (NumInternalRetry == 2) {
        if (error == LastError) {
            LastError = error;
            return 1;
        } else {
            LastError = error;
            return 2;
        }
    } else {
        LastError = error;

        if ((error == 0x31100) || (executing->command == DVD_COMMAND_READID)) {
            return 2;
        } else {
            return 3;
        }
    }
}

static inline BOOL CheckCancel(u32 resume) {
    DVDCommandBlock* finished;

    if (Canceling) {
        ResumeFromHere = resume;
        Canceling = FALSE;

        finished = executing;
        executing = &DummyCommandBlock;

        finished->state = DVD_STATE_CANCELED;
        if (finished->callback) {
            (*finished->callback)(DVD_RESULT_CANCELED, finished);
        }
        if (CancelCallback) {
            (CancelCallback)(0, finished);
        }
        stateReady();
        return TRUE;
    }
    return FALSE;
}

static void cbForStateGettingError(u32 intType) {
    u32 error;
    u32 status;
    u32 errorCategory;
    u32 resume;

    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 2) {
        Unrecoverable(0x01234567);
        return;
    }

    error = __DIRegs[DI_IMMBUF];
    status = error & 0xFF000000;

    errorCategory = CategorizeError(error);

    if (errorCategory == 1) {
        Unrecoverable(error);
        return;
    }

    if ((errorCategory == 2) || (errorCategory == 3)) {
        resume = 0;
    } else {
        if (status == 0x01000000) {
            resume = 4;
        } else if (status == 0x02000000) {
            resume = 6;
        } else if (status == 0x03000000) {
            resume = 3;
        } else {
            resume = 5;
        }
    }

    if (CheckCancel(resume)) {
        return;
    }

    if (errorCategory == 2) {
        __DVDStoreErrorCode(error);
        stateGoToRetry();
        return;
    }

    if (errorCategory == 3) {
        if ((error & 0x00FFFFFF) == 0x00031100) {
            DVDLowSeek(executing->offset, cbForUnrecoveredError);
        } else {
            LastState(executing);
        }
        return;
    }

    if (status == 0x01000000) {
        executing->state = DVD_STATE_COVER_OPEN;
        stateMotorStopped();
    } else if (status == 0x02000000) {
        executing->state = DVD_STATE_COVER_CLOSED;
        stateCoverClosed();
    } else if (status == 0x03000000) {
        executing->state = DVD_STATE_NO_DISK;
        stateMotorStopped();
    } else {
        Unrecoverable(0x01234567);
    }
}

static void cbForUnrecoveredError(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 1) {
        stateGoToRetry();
        return;
    }

    DVDLowRequestError(cbForUnrecoveredErrorRetry);
}

static void cbForUnrecoveredErrorRetry(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    executing->state = DVD_STATE_FATAL_ERROR;

    if (intType & 2) {
        __DVDStoreErrorCode(0x01234567);
        DVDLowStopMotor(cbForStateError);
        return;
    }

    __DVDStoreErrorCode(__DIRegs[DI_IMMBUF]);
    DVDLowStopMotor(cbForStateError);
}

static void stateGoToRetry(void) {
    DVDLowStopMotor(cbForStateGoToRetry);
}

static void cbForStateGoToRetry(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 2) {
        Unrecoverable(0x01234567);
        return;
    }

    NumInternalRetry = 0;

    if ((CurrCommand == DVD_COMMAND_BSREAD) || (CurrCommand == DVD_COMMAND_READID) ||
        (CurrCommand == DVD_COMMAND_AUDIO_BUFFER_CONFIG) || (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK)) {
        ResetRequired = TRUE;
    }

    if (!CheckCancel(2)) {
        executing->state = DVD_STATE_RETRY;
        stateMotorStopped();
    }
}

static void stateCheckID(void) {
    switch (CurrCommand) {
    case DVD_COMMAND_CHANGE_DISK:
        if (memcmp(tmpBuffer, executing->id, 0x1C) != 0) {
            DVDLowStopMotor(cbForStateCheckID1);
        } else {
            memcpy(currID, tmpBuffer, sizeof(DVDDiskID));
            executing->state = DVD_STATE_BUSY;
            DCInvalidateRange(tmpBuffer, sizeof(DVDBB2));
            LastState = stateCheckID2;
            stateCheckID2(executing);
        }
        break;

    default:
        if (memcmp(tmpBuffer, currID, sizeof(DVDDiskID)) != 0) {
            DVDLowStopMotor(cbForStateCheckID1);
        } else {
            LastState = stateCheckID3;
            stateCheckID3(executing);
        }
        break;
    }
}

static void stateCheckID3(DVDCommandBlock* block) {
    DVDLowAudioBufferConfig(currID->streaming, 10, cbForStateCheckID3);
}

static void stateCheckID2(DVDCommandBlock* block) {
    DVDLowRead(tmpBuffer, (sizeof(DVDBB2) + 31) & ~31, 0x420, cbForStateCheckID2);
}

static void cbForStateCheckID1(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 2) {
        Unrecoverable(0x01234567);
        return;
    }

    NumInternalRetry = 0;

    if (!CheckCancel(1)) {
        executing->state = DVD_STATE_WRONG_DISK;
        stateMotorStopped();
    }
}

static void cbForStateCheckID2(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        stateReadingFST();
    } else {
        stateGettingError();
    }
}

static void cbForStateCheckID3(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;

        if (!CheckCancel(0)) {
            executing->state = DVD_STATE_BUSY;
            stateBusy(executing);
        }
    } else {
        stateGettingError();
    }
}

static void AlarmHandler(OSAlarm* alarm, OSContext* context) {
    DVDReset();
    DCInvalidateRange(tmpBuffer, sizeof(DVDDiskID));
    LastState = stateCoverClosed_CMD;
    stateCoverClosed_CMD(executing);
}

static void stateCoverClosed(void) {
    DVDCommandBlock* finished;

    switch (CurrCommand) {
    case DVD_COMMAND_READID:
    case DVD_COMMAND_BSREAD:
    case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
    case DVD_COMMAND_BS_CHANGE_DISK:
        __DVDClearWaitingQueue();
        finished = executing;
        executing = &DummyCommandBlock;
        if (finished->callback) {
            (finished->callback)(DVD_RESULT_COVER_CLOSED, finished);
        }
        stateReady();
        break;

    default:
        // Give the drive time to spin up before reading the disc ID.
        DVDReset();
        OSCreateAlarm(&ResetAlarm);
        OSSetAlarm(&ResetAlarm, OSMillisecondsToTicks(1150), AlarmHandler);
        break;
    }
}

static void stateCoverClosed_CMD(DVDCommandBlock* block) {
    DVDLowReadDiskID((DVDDiskID*)tmpBuffer, cbForStateCoverClosed);
}

static void cbForStateCoverClosed(u32 intType) {
    if (intType == 16) {
        TimedOut();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        stateCheckID();
    } else {
        stateGettingError();
    }
}

static void stateMotorStopped(void) {
    DVDLowWaitCoverClose(cbForStateMotorStopped);
}

static void cbForStateMotorStopped(u32 intType) {
    __DIRegs[DI_COVER_STATUS] = 0;
    executing->state = DVD_STATE_COVER_CLOSED;
    stateCoverClosed();
}

static void stateReady(void) {
    DVDCommandBlock* finished;

    if (!__DVDCheckWaitingQueue()) {
        executing = NULL;
        return;
    }

    if (PauseFlag) {
        PausingFlag = TRUE;
        executing = NULL;
        return;
    }

    executing = __DVDPopWaitingQueue();

    if (FatalErrorFlag) {
        executing->state = DVD_STATE_FATAL_ERROR;
        finished = executing;
        executing = &DummyCommandBlock;
        if (finished->callback) {
            (finished->callback)(DVD_RESULT_FATAL_ERROR, finished);
        }
        stateReady();
        return;
    }

    CurrCommand = executing->command;

    if (ResumeFromHere) {
        switch (ResumeFromHere) {
        case 1:
            executing->state = DVD_STATE_WRONG_DISK;
            stateMotorStopped();
            break;
        case 2:
            executing->state = DVD_STATE_RETRY;
            stateMotorStopped();
            break;
        case 3:
            executing->state = DVD_STATE_NO_DISK;
            stateMotorStopped();
            break;
        case 7:
            executing->state = DVD_STATE_MOTOR_STOPPED;
            stateMotorStopped();
            break;
        case 4:
            executing->state = DVD_STATE_COVER_OPEN;
            stateMotorStopped();
            break;
        case 6:
            executing->state = DVD_STATE_COVER_CLOSED;
            stateCoverClosed();
            break;
        case 5:
            executing->state = DVD_STATE_FATAL_ERROR;
            __DVDStoreErrorCode(CancelLastError);
            DVDLowStopMotor(cbForStateError);
            break;
        }
        ResumeFromHere = 0;
    } else {
        executing->state = DVD_STATE_BUSY;
        stateBusy(executing);
    }
}

#define MIN(a, b) (((a) > (b)) ? (b) : (a))

static void stateBusy(DVDCommandBlock* block) {
    LastState = stateBusy;

    switch (block->command) {
    case DVD_COMMAND_READID:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        block->currTransferSize = sizeof(DVDDiskID);
        DVDLowReadDiskID((DVDDiskID*)block->addr, cbForStateBusy);
        break;

    case DVD_COMMAND_READ:
    case DVD_COMMAND_BSREAD:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        block->currTransferSize = MIN(block->length - block->transferredSize, 0x80000);
        DVDLowRead((void*)((u8*)block->addr + block->transferredSize), block->currTransferSize,
                   block->offset + block->transferredSize, cbForStateBusy);
        break;

    case DVD_COMMAND_SEEK:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowSeek(block->offset, cbForStateBusy);
        break;

    case DVD_COMMAND_CHANGE_DISK:
        DVDLowStopMotor(cbForStateBusy);
        break;

    case DVD_COMMAND_BS_CHANGE_DISK:
        DVDLowStopMotor(cbForStateBusy);
        break;

    case DVD_COMMAND_INITSTREAM:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        if (AutoFinishing) {
            executing->currTransferSize = 0;
            DVDLowRequestAudioStatus(0, cbForStateBusy);
        } else {
            executing->currTransferSize = 1;
            DVDLowAudioStream(0, block->length, block->offset, cbForStateBusy);
        }
        break;

    case DVD_COMMAND_CANCELSTREAM:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowAudioStream(0x00010000, 0, 0, cbForStateBusy);
        break;

    case DVD_COMMAND_STOP_STREAM_AT_END:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        AutoFinishing = TRUE;
        DVDLowAudioStream(0, 0, 0, cbForStateBusy);
        break;

    case DVD_COMMAND_REQUEST_AUDIO_ERROR:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowRequestAudioStatus(0, cbForStateBusy);
        break;

    case DVD_COMMAND_REQUEST_PLAY_ADDR:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowRequestAudioStatus(0x00010000, cbForStateBusy);
        break;

    case DVD_COMMAND_REQUEST_START_ADDR:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowRequestAudioStatus(0x00020000, cbForStateBusy);
        break;

    case DVD_COMMAND_REQUEST_LENGTH:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowRequestAudioStatus(0x00030000, cbForStateBusy);
        break;

    case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        DVDLowAudioBufferConfig(block->offset, block->length, cbForStateBusy);
        break;

    case DVD_COMMAND_INQUIRY:
        __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
        block->currTransferSize = 0x20;
        DVDLowInquiry(block->addr, cbForStateBusy);
        break;
    }
}

static BOOL IsDmaCommand(u32 command) {
    switch (command) {
    case DVD_COMMAND_READ:
    case DVD_COMMAND_BSREAD:
    case DVD_COMMAND_READID:
    case DVD_COMMAND_INQUIRY:
        return TRUE;
    default:
        return FALSE;
    }
}

static void cbForStateBusy(u32 intType) {
    DVDCommandBlock* finished;

    if (intType == 16) {
        TimedOut();
        return;
    }

    if ((CurrCommand == DVD_COMMAND_CHANGE_DISK) || (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK)) {
        if (intType & 2) {
            Unrecoverable(0x01234567);
            return;
        }

        NumInternalRetry = 0;

        if (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK) {
            ResetRequired = TRUE;
        }

        if (CheckCancel(7)) {
            return;
        }

        executing->state = DVD_STATE_MOTOR_STOPPED;
        stateMotorStopped();
        return;
    }

    if (IsDmaCommand(CurrCommand)) {
        executing->transferredSize += executing->currTransferSize - __DIRegs[DI_LENGTH];
    }

    if (intType & 8) {
        Canceling = FALSE;
        finished = executing;
        executing = &DummyCommandBlock;

        finished->state = DVD_STATE_CANCELED;
        if (finished->callback) {
            (*finished->callback)(DVD_RESULT_CANCELED, finished);
        }
        if (CancelCallback) {
            (CancelCallback)(0, finished);
        }

        stateReady();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;

        if (CheckCancel(0)) {
            return;
        }

        if (IsDmaCommand(CurrCommand)) {
            if (executing->transferredSize != executing->length) {
                stateBusy(executing);
                return;
            }

            finished = executing;
            executing = &DummyCommandBlock;

            finished->state = DVD_STATE_END;
            if (finished->callback) {
                (finished->callback)((s32)finished->transferredSize, finished);
            }
            stateReady();
        } else if ((CurrCommand == DVD_COMMAND_REQUEST_AUDIO_ERROR) || (CurrCommand == DVD_COMMAND_REQUEST_PLAY_ADDR) ||
                   (CurrCommand == DVD_COMMAND_REQUEST_START_ADDR) || (CurrCommand == DVD_COMMAND_REQUEST_LENGTH)) {
            s32 result;

            if ((CurrCommand == DVD_COMMAND_REQUEST_START_ADDR) || (CurrCommand == DVD_COMMAND_REQUEST_PLAY_ADDR)) {
                result = (s32)(__DIRegs[DI_IMMBUF] << 2);
            } else {
                result = (s32)__DIRegs[DI_IMMBUF];
            }
            finished = executing;
            executing = &DummyCommandBlock;

            finished->state = DVD_STATE_END;
            if (finished->callback) {
                (finished->callback)(result, finished);
            }
            stateReady();
        } else if (CurrCommand == DVD_COMMAND_INITSTREAM) {
            if (executing->currTransferSize == 0) {
                if (__DIRegs[DI_IMMBUF] & 1) {
                    finished = executing;
                    executing = &DummyCommandBlock;

                    finished->state = DVD_STATE_IGNORED;
                    if (finished->callback) {
                        (finished->callback)(DVD_RESULT_IGNORED, finished);
                    }
                    stateReady();
                } else {
                    AutoFinishing = FALSE;
                    executing->currTransferSize = 1;
                    DVDLowAudioStream(0, executing->length, executing->offset, cbForStateBusy);
                }
            } else {
                finished = executing;
                executing = &DummyCommandBlock;

                finished->state = DVD_STATE_END;
                if (finished->callback) {
                    (finished->callback)(0, finished);
                }
                stateReady();
            }
        } else {
            finished = executing;
            executing = &DummyCommandBlock;

            finished->state = DVD_STATE_END;
            if (finished->callback) {
                (finished->callback)(0, finished);
            }
            stateReady();
        }
    } else {
        if (CurrCommand == DVD_COMMAND_INQUIRY) {
            Unrecoverable(0x01234567);
            return;
        }

        if (IsDmaCommand(CurrCommand) && (executing->transferredSize == executing->length)) {
            if (CheckCancel(0)) {
                return;
            }
            finished = executing;
            executing = &DummyCommandBlock;

            finished->state = DVD_STATE_END;
            if (finished->callback) {
                (finished->callback)((s32)finished->transferredSize, finished);
            }
            stateReady();
            return;
        }

        stateGettingError();
    }
}

static BOOL issueCommand(s32 prio, DVDCommandBlock* block) {
    BOOL level;
    BOOL result;

    if (autoInvalidation && IsDmaCommand(block->command)) {
        DCInvalidateRange(block->addr, block->length);
    }

    level = OSDisableInterrupts();

    block->state = DVD_STATE_WAITING;
    result = __DVDPushWaitingQueue(prio, block);

    if ((executing == NULL) && (PauseFlag == FALSE)) {
        stateReady();
    }

    OSRestoreInterrupts(level);
    return result;
}

BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback,
                         s32 prio) {
    BOOL idle;

    block->command = DVD_COMMAND_READ;
    block->addr = addr;
    block->length = length;
    block->offset = offset;
    block->transferredSize = 0;
    block->callback = callback;

    idle = issueCommand(prio, block);
    return idle;
}

BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback) {
    BOOL idle;

    block->command = DVD_COMMAND_BSREAD;
    block->addr = addr;
    block->length = length;
    block->offset = offset;
    block->transferredSize = 0;
    block->callback = callback;

    idle = issueCommand(2, block);
    return idle;
}

BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback) {
    BOOL idle;

    block->command = DVD_COMMAND_READID;
    block->addr = diskID;
    block->length = sizeof(DVDDiskID);
    block->offset = 0;
    block->transferredSize = 0;
    block->callback = callback;

    idle = issueCommand(2, block);
    return idle;
}

void DVDReset(void) {
    DVDLowReset();
    __DIRegs[DI_STATUS] = 0x2A;
    __DIRegs[DI_COVER_STATUS] = __DIRegs[DI_COVER_STATUS];
    ResetRequired = FALSE;
    ResumeFromHere = 0;
}

s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) {
    BOOL enabled;
    s32 retVal;

    enabled = OSDisableInterrupts();
    if (block->state == DVD_STATE_COVER_CLOSED) {
        retVal = DVD_STATE_BUSY;
    } else {
        retVal = block->state;
    }
    OSRestoreInterrupts(enabled);
    return retVal;
}

s32 DVDGetDriveStatus(void) {
    BOOL enabled;
    s32 retVal;

    enabled = OSDisableInterrupts();

    if (FatalErrorFlag) {
        retVal = DVD_STATE_FATAL_ERROR;
    } else if (PausingFlag) {
        retVal = DVD_STATE_PAUSING;
    } else if (executing == NULL) {
        retVal = DVD_STATE_END;
    } else if (executing == &DummyCommandBlock) {
        retVal = DVD_STATE_END;
    } else {
        retVal = DVDGetCommandBlockStatus(executing);
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

BOOL DVDSetAutoInvalidation(BOOL autoInval) {
    BOOL prev;

    prev = autoInvalidation;
    autoInvalidation = autoInval;
    return prev;
}

static inline void DVDPause(void) {
    BOOL level;

    level = OSDisableInterrupts();
    PauseFlag = TRUE;
    if (executing == NULL) {
        PausingFlag = TRUE;
    }
    OSRestoreInterrupts(level);
}

static inline void DVDResume(void) {
    BOOL level;

    level = OSDisableInterrupts();
    PauseFlag = FALSE;
    if (PausingFlag) {
        PausingFlag = FALSE;
        stateReady();
    }
    OSRestoreInterrupts(level);
}

BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    BOOL enabled;
    DVDLowCallback old;

    enabled = OSDisableInterrupts();

    switch (block->state) {
    case DVD_STATE_FATAL_ERROR:
    case DVD_STATE_END:
    case DVD_STATE_CANCELED:
        if (callback) {
            (*callback)(0, block);
        }
        break;

    case DVD_STATE_BUSY:
        if (Canceling) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }

        Canceling = TRUE;
        CancelCallback = callback;
        if ((block->command == DVD_COMMAND_BSREAD) || (block->command == DVD_COMMAND_READ)) {
            DVDLowBreak();
        }
        break;

    case DVD_STATE_WAITING:
        __DVDDequeueWaitingQueue(block);
        block->state = DVD_STATE_CANCELED;
        if (block->callback) {
            (block->callback)(DVD_RESULT_CANCELED, block);
        }
        if (callback) {
            (*callback)(0, block);
        }
        break;

    case DVD_STATE_COVER_CLOSED:
        switch (block->command) {
        case DVD_COMMAND_READID:
        case DVD_COMMAND_BSREAD:
        case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
        case DVD_COMMAND_BS_CHANGE_DISK:
            if (callback) {
                (*callback)(0, block);
            }
            break;

        default:
            if (Canceling) {
                OSRestoreInterrupts(enabled);
                return FALSE;
            }
            Canceling = TRUE;
            CancelCallback = callback;
            break;
        }
        break;

    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
    case DVD_STATE_MOTOR_STOPPED:
    case DVD_STATE_RETRY:
        old = DVDLowClearCallback();
        if (old != cbForStateMotorStopped) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }

        if (block->state == DVD_STATE_NO_DISK) {
            ResumeFromHere = 3;
        }
        if (block->state == DVD_STATE_COVER_OPEN) {
            ResumeFromHere = 4;
        }
        if (block->state == DVD_STATE_WRONG_DISK) {
            ResumeFromHere = 1;
        }
        if (block->state == DVD_STATE_RETRY) {
            ResumeFromHere = 2;
        }
        if (block->state == DVD_STATE_MOTOR_STOPPED) {
            ResumeFromHere = 7;
        }

        block->state = DVD_STATE_CANCELED;
        if (block->callback) {
            (block->callback)(DVD_RESULT_CANCELED, block);
        }
        if (callback) {
            (callback)(0, block);
        }
        stateReady();
        break;
    }

    OSRestoreInterrupts(enabled);
    return TRUE;
}

static void cbForCancelSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

s32 DVDCancel(DVDCommandBlock* block) {
    BOOL result;
    s32 state;
    u32 command;
    BOOL enabled;

    result = DVDCancelAsync(block, cbForCancelSync);
    if (result == FALSE) {
        return -1;
    }

    enabled = OSDisableInterrupts();

    for (;;) {
        state = ((volatile DVDCommandBlock*)block)->state;

        if ((state == DVD_STATE_END) || (state == DVD_STATE_FATAL_ERROR) || (state == DVD_STATE_CANCELED)) {
            break;
        }

        if (state == DVD_STATE_COVER_CLOSED) {
            command = ((volatile DVDCommandBlock*)block)->command;
            if ((command == DVD_COMMAND_BSREAD) || (command == DVD_COMMAND_READID) ||
                (command == DVD_COMMAND_AUDIO_BUFFER_CONFIG) || (command == DVD_COMMAND_BS_CHANGE_DISK)) {
                break;
            }
        }

        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return 0;
}

DVDDiskID* DVDGetCurrentDiskID(void) {
    return (DVDDiskID*)0x80000000;
}

BOOL DVDCheckDisk(void) {
    BOOL enabled;
    s32 retVal;
    s32 state;
    u32 coverReg;

    enabled = OSDisableInterrupts();

    if (FatalErrorFlag) {
        state = DVD_STATE_FATAL_ERROR;
    } else if (PausingFlag) {
        state = DVD_STATE_PAUSING;
    } else if (executing == NULL) {
        state = DVD_STATE_END;
    } else if (executing == &DummyCommandBlock) {
        state = DVD_STATE_END;
    } else {
        state = executing->state;
    }

    switch (state) {
    case DVD_STATE_BUSY:
    case DVD_STATE_IGNORED:
    case DVD_STATE_CANCELED:
    case DVD_STATE_WAITING:
        retVal = TRUE;
        break;

    case DVD_STATE_FATAL_ERROR:
    case DVD_STATE_RETRY:
    case DVD_STATE_MOTOR_STOPPED:
    case DVD_STATE_COVER_CLOSED:
    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
        retVal = FALSE;
        break;

    case DVD_STATE_END:
    case DVD_STATE_PAUSING:
        coverReg = __DIRegs[DI_COVER_STATUS];
        if (((coverReg >> 2) & 1) || (coverReg & 1)) {
            retVal = FALSE;
        } else {
            retVal = TRUE;
        }
        break;
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

void __DVDPrepareResetAsync(DVDCBCallback callback) {
    BOOL enabled;
    DVDCommandBlock* p;

    enabled = OSDisableInterrupts();

    __DVDClearWaitingQueue();

    if (Canceling) {
        CancelCallback = callback;
    } else {
        if (executing) {
            executing->callback = NULL;
        }

        // DVDCancelAllAsync(callback)
        {
            BOOL enabled2 = OSDisableInterrupts();

            DVDPause();
            while ((p = __DVDPopWaitingQueue()) != NULL) {
                DVDCancelAsync(p, NULL);
            }

            if (executing) {
                DVDCancelAsync(executing, callback);
            } else if (callback) {
                (*callback)(0, NULL);
            }

            DVDResume();
            OSRestoreInterrupts(enabled2);
        }
    }

    OSRestoreInterrupts(enabled);
}
