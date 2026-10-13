#include <TRK_MINNOW_DOLPHIN/trk.h>

// DSReplyError
#define kDSReplyNoError 0x00
#define kDSReplyError 0x01
#define kDSReplyPacketSizeError 0x02
#define kDSReplyCWDSError 0x03
#define kDSReplyUnsupportedCommandError 0x10
#define kDSReplyParameterError 0x11
#define kDSReplyUnsupportedOptionError 0x12
#define kDSReplyInvalidMemoryRange 0x13
#define kDSReplyInvalidRegisterRange 0x14
#define kDSReplyCWDSException 0x15
#define kDSReplyNotStopped 0x16
#define kDSReplyOsError 0x20
#define kDSReplyInvalidProcessId 0x21
#define kDSReplyInvalidThreadId 0x22

// Memory command options
#define DS_MSG_MEMORY_EXTENDED 0x02
#define DS_MSG_MEMORY_USERVIEW 0x08

// Register sets
#define kDSRegistersDefault 0
#define kDSRegistersFP 1
#define kDSRegistersExtended1 2
#define kDSRegistersExtended2 3

// Step modes
#define kDSStepIntoCount 0x00
#define kDSStepIntoRange 0x01
#define kDSStepOverCount 0x10
#define kDSStepOverRange 0x11

#define kDSOption_SerialIO 1

static BOOL IsTRKConnected;

BOOL GetTRKConnected(void) {
    return IsTRKConnected;
}

void SetTRKConnected(BOOL connected) {
    IsTRKConnected = connected;
}

static inline void TRKMessageIntoReply(TRKBuffer* buffer, u8 ackCmd, u8 errSentInAck) {
    TRKResetBuffer(buffer, 1);
    TRKAppendBuffer1_ui8(buffer, ackCmd);
    TRKAppendBuffer1_ui8(buffer, errSentInAck);
}

static inline DSError TRKSendACK(TRKBuffer* buffer) {
    DSError err;
    int ackTries;

    ackTries = 3;
    do {
        err = TRKMessageSend(buffer);
        --ackTries;
    } while (err != kNoError && ackTries > 0);

    return err;
}

DSError TRKStandardACK(TRKBuffer* buffer, MessageCommandID commandID, int replyError) {
    TRKMessageIntoReply(buffer, commandID, replyError);
    return TRKSendACK(buffer);
}

DSError TRKDoUnsupported(TRKBuffer* buffer) {
    return TRKStandardACK(buffer, kDSReplyACK, kDSReplyUnsupportedCommandError);
}

DSError TRKDoConnect(TRKBuffer* buffer) {
    IsTRKConnected = TRUE;
    return TRKStandardACK(buffer, kDSReplyACK, kDSReplyNoError);
}

DSError TRKDoDisconnect(TRKBuffer* buffer) {
    DSError error;
    TRKEvent event;

    IsTRKConnected = FALSE;
    if ((error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyNoError)) == kNoError) {
        TRKConstructEvent(&event, kShutdownEvent);
        TRKPostEvent(&event);
    }
    return error;
}

DSError TRKDoReset(TRKBuffer* buffer) {
    TRKStandardACK(buffer, kDSReplyACK, kDSReplyNoError);
    __TRK_reset();
    return kNoError;
}

DSError TRKDoVersions(TRKBuffer* buffer) {
    DSError error;
    DSVersions versions;

    if (buffer->length != 1) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
    } else {
        TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
        error = TRKTargetVersions(&versions);

        if (error == kNoError) {
            error = TRKAppendBuffer1_ui8(buffer, versions.kernelMajor);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui8(buffer, versions.kernelMinor);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui8(buffer, versions.protocolMajor);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui8(buffer, versions.protocolMinor);
        }

        if (error != kNoError) {
            error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyCWDSError);
        } else {
            error = TRKSendACK(buffer);
        }
    }

    return error;
}

DSError TRKDoSupportMask(TRKBuffer* buffer) {
    DSError error;
    u8 mask[32];

    if (buffer->length != 1) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
    } else {
        TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
        error = TRKTargetSupportMask(mask);

        if (error == kNoError) {
            error = TRKAppendBuffer(buffer, mask, 32);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui8(buffer, 2);  // protocol level
        }

        if (error != kNoError) {
            error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyCWDSError);
        } else {
            error = TRKSendACK(buffer);
        }
    }

    return error;
}

DSError TRKDoCPUType(TRKBuffer* buffer) {
    DSError error;
    DSCPUType cputype;

    if (buffer->length != 1) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
    error = TRKTargetCPUType(&cputype);

    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.cpuMajor);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.cpuMinor);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.bigEndian);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.defaultTypeSize);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.fpTypeSize);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.extended1TypeSize);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, cputype.extended2TypeSize);
    }

    if (error != kNoError) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyCWDSError);
    } else {
        error = TRKSendACK(buffer);
    }

    return error;
}

static inline int MemoryErrorToReply(DSError error) {
    switch (error) {
    case kCWDSException:
        return kDSReplyCWDSException;
    case kInvalidMemory:
        return kDSReplyInvalidMemoryRange;
    case kInvalidProcessId:
        return kDSReplyInvalidProcessId;
    case kInvalidThreadId:
        return kDSReplyInvalidThreadId;
    case kOsError:
        return kDSReplyOsError;
    default:
        return kDSReplyCWDSError;
    }
}

DSError TRKDoReadMemory(TRKBuffer* buffer) {
    DSError error;
    int replyError;
    u8 tempBuf[0x800];
    u32 length;
    u32 msg_start;
    u16 msg_length;
    u8 msg_options;
    u8 msg_command;

    if (buffer->length != 8) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &msg_command);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &msg_options);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui16(buffer, &msg_length);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui32(buffer, &msg_start);
    }

    if (msg_options & DS_MSG_MEMORY_EXTENDED) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyUnsupportedOptionError);
        return error;
    }

    if (msg_length > 0x800) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyParameterError);
        return error;
    }

    TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);

    if (error == kNoError) {
        length = msg_length;
        error = TRKTargetAccessMemory(tempBuf, msg_start, &length,
                                      (msg_options & DS_MSG_MEMORY_USERVIEW) ? kUserMemory : kDebuggerMemory, TRUE);
        msg_length = length;
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui16(buffer, msg_length);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer(buffer, tempBuf, length);
        }
    }

    if (error != kNoError) {
        replyError = MemoryErrorToReply(error);
        error = TRKStandardACK(buffer, kDSReplyACK, replyError);
    } else {
        error = TRKSendACK(buffer);
    }

    return error;
}

DSError TRKDoWriteMemory(TRKBuffer* buffer) {
    DSError error;
    int replyError;
    u8 tmpBuffer[0x800];
    u32 length;
    u32 msg_start;
    u16 msg_length;
    u8 msg_options;
    u8 msg_command;

    if (buffer->length <= 8) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &msg_command);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &msg_options);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui16(buffer, &msg_length);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui32(buffer, &msg_start);
    }

    if (msg_options & DS_MSG_MEMORY_EXTENDED) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyUnsupportedOptionError);
        return error;
    }

    if (buffer->length != msg_length + 8 || msg_length > 0x800) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyParameterError);
        return error;
    }

    if (error == kNoError) {
        length = msg_length;
        error = TRKReadBuffer(buffer, tmpBuffer, length);
        if (error == kNoError) {
            error = TRKTargetAccessMemory(tmpBuffer, msg_start, &length,
                                          (msg_options & DS_MSG_MEMORY_USERVIEW) ? kUserMemory : kDebuggerMemory,
                                          FALSE);
        }
        msg_length = length;
    }

    if (error == kNoError) {
        TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui16(buffer, msg_length);
    }

    if (error != kNoError) {
        replyError = MemoryErrorToReply(error);
        error = TRKStandardACK(buffer, kDSReplyACK, replyError);
    } else {
        error = TRKSendACK(buffer);
    }

    return error;
}

DSError TRKDoReadRegisters(TRKBuffer* buffer) {
    DSError error;
    int replyError;
    u32 registerDataLength;
    u16 msg_firstRegister;
    u16 msg_lastRegister;
    u8 msg_options;
    u8 msg_command;

    if (buffer->length != 6) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &msg_command);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &msg_options);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui16(buffer, &msg_firstRegister);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui16(buffer, &msg_lastRegister);
    }

    if (msg_firstRegister > msg_lastRegister) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyInvalidRegisterRange);
        return error;
    }

    if (error == kNoError) {
        TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
    }

    switch (msg_options & 7) {
    case kDSRegistersDefault:
        error = TRKTargetAccessDefault(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, TRUE);
        break;
    case kDSRegistersFP:
        error = TRKTargetAccessFP(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, TRUE);
        break;
    case kDSRegistersExtended1:
        error = TRKTargetAccessExtended1(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, TRUE);
        break;
    case kDSRegistersExtended2:
        error = TRKTargetAccessExtended2(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, TRUE);
        break;
    default:
        error = kUnsupportedError;
        break;
    }

    if (error != kNoError) {
        switch (error) {
        case kUnsupportedError:
            replyError = kDSReplyUnsupportedOptionError;
            break;
        case kInvalidRegister:
            replyError = kDSReplyInvalidRegisterRange;
            break;
        case kCWDSException:
            replyError = kDSReplyCWDSException;
            break;
        case kInvalidProcessId:
            replyError = kDSReplyInvalidProcessId;
            break;
        case kInvalidThreadId:
            replyError = kDSReplyInvalidThreadId;
            break;
        case kOsError:
            replyError = kDSReplyOsError;
            break;
        default:
            replyError = kDSReplyCWDSError;
            break;
        }
        error = TRKStandardACK(buffer, kDSReplyACK, replyError);
    } else {
        error = TRKSendACK(buffer);
    }

    return error;
}

DSError TRKDoWriteRegisters(TRKBuffer* buffer) {
    DSError error;
    int replyError;
    u32 registerDataLength;
    u16 msg_firstRegister;
    u16 msg_lastRegister;
    u8 msg_options;
    u8 msg_command;

    if (buffer->length <= 6) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &msg_command);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &msg_options);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui16(buffer, &msg_firstRegister);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui16(buffer, &msg_lastRegister);
    }

    if (msg_firstRegister > msg_lastRegister) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyInvalidRegisterRange);
        return error;
    }

    switch (msg_options) {
    case kDSRegistersDefault:
        error = TRKTargetAccessDefault(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, FALSE);
        break;
    case kDSRegistersFP:
        error = TRKTargetAccessFP(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, FALSE);
        break;
    case kDSRegistersExtended1:
        error = TRKTargetAccessExtended1(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, FALSE);
        break;
    case kDSRegistersExtended2:
        error = TRKTargetAccessExtended2(msg_firstRegister, msg_lastRegister, buffer, &registerDataLength, FALSE);
        break;
    default:
        error = kUnsupportedError;
        break;
    }

    if (error == kNoError) {
        TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
    }

    if (error != kNoError) {
        switch (error) {
        case kUnsupportedError:
            replyError = kDSReplyUnsupportedOptionError;
            break;
        case kInvalidRegister:
            replyError = kDSReplyInvalidRegisterRange;
            break;
        case kMessageBufferReadError:
            replyError = kDSReplyPacketSizeError;
            break;
        case kCWDSException:
            replyError = kDSReplyCWDSException;
            break;
        case kInvalidProcessId:
            replyError = kDSReplyInvalidProcessId;
            break;
        case kInvalidThreadId:
            replyError = kDSReplyInvalidThreadId;
            break;
        case kOsError:
            replyError = kDSReplyOsError;
            break;
        default:
            replyError = kDSReplyCWDSError;
            break;
        }
        error = TRKStandardACK(buffer, kDSReplyACK, replyError);
    } else {
        error = TRKSendACK(buffer);
    }

    return error;
}

DSError TRKDoFlushCache(TRKBuffer* buffer) {
    DSError error;
    int replyErr;
    u32 msg_start;
    u32 msg_end;
    u8 msg_options;
    u8 msg_command;

    if (buffer->length != 10) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &msg_command);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &msg_options);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui32(buffer, &msg_start);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui32(buffer, &msg_end);
    }

    if (msg_start > msg_end) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyInvalidMemoryRange);
        return error;
    }

    if (error == kNoError) {
        error = TRKTargetFlushCache(msg_options, (void*)msg_start, msg_end);
    }
    if (error == kNoError) {
        TRKMessageIntoReply(buffer, kDSReplyACK, kDSReplyNoError);
    }

    if (error != kNoError) {
        switch (error) {
        case kUnsupportedError:
            replyErr = kDSReplyUnsupportedOptionError;
            break;
        default:
            replyErr = kDSReplyCWDSError;
            break;
        }
        error = TRKStandardACK(buffer, kDSReplyACK, replyErr);
    } else {
        error = TRKSendACK(buffer);
    }

    return error;
}

DSError TRKDoContinue(TRKBuffer* buffer) {
    DSError error;

    if (!TRKTargetStopped()) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyNotStopped);
        return error;
    }

    error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyNoError);
    if (error == kNoError) {
        TRKTargetContinue();
    }
    return error;
}

DSError TRKDoStep(TRKBuffer* buffer) {
    DSError error;
    u8 msg_command;
    u8 msg_options;
    u8 msg_count;
    u32 msg_rangeStart;
    u32 msg_rangeEnd;
    u32 pc;

    if (buffer->length < 3) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
        return error;
    }

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &msg_command);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &msg_options);
    }

    switch (msg_options) {
    case kDSStepIntoCount:
    case kDSStepOverCount:
        if (error == kNoError) {
            error = TRKReadBuffer1_ui8(buffer, &msg_count);
        }
        if (msg_count >= 1) {
            break;
        }
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyParameterError);
        return error;
    case kDSStepIntoRange:
    case kDSStepOverRange:
        if (buffer->length != 10) {
            error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyPacketSizeError);
            return error;
        }

        if (error == kNoError) {
            error = TRKReadBuffer1_ui32(buffer, &msg_rangeStart);
        }
        if (error == kNoError) {
            error = TRKReadBuffer1_ui32(buffer, &msg_rangeEnd);
        }

        pc = TRKTargetGetPC();
        if (pc >= msg_rangeStart && pc <= msg_rangeEnd) {
            break;
        }
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyParameterError);
        return error;
    default:
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyUnsupportedOptionError);
        return error;
    }

    if (!TRKTargetStopped()) {
        error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyNotStopped);
        return error;
    }

    error = TRKStandardACK(buffer, kDSReplyACK, kDSReplyNoError);
    if (error == kNoError) {
        switch (msg_options) {
        case kDSStepIntoCount:
        case kDSStepOverCount:
            error = TRKTargetSingleStep(msg_count, msg_options == kDSStepOverCount);
            break;
        case kDSStepIntoRange:
        case kDSStepOverRange:
            error = TRKTargetStepOutOfRange(msg_rangeStart, msg_rangeEnd, msg_options == kDSStepOverRange);
            break;
        }
    }

    return error;
}

DSError TRKDoStop(TRKBuffer* b) {
    int replyError;

    switch (TRKTargetStop()) {
    case kNoError:
        replyError = kDSReplyNoError;
        break;
    case kInvalidProcessId:
        replyError = kDSReplyInvalidProcessId;
        break;
    case kInvalidThreadId:
        replyError = kDSReplyInvalidThreadId;
        break;
    case kOsError:
        replyError = kDSReplyOsError;
        break;
    default:
        replyError = kDSReplyError;
        break;
    }

    return TRKStandardACK(b, kDSReplyACK, replyError);
}

DSError TRKDoSetOption(TRKBuffer* buffer) {
    DSError error;
    u8 spA;
    u8 sp9;
    u8 sp8;

    spA = 0;
    sp9 = 0;
    sp8 = 0;

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &spA);
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &sp9);
    }
    if (error == kNoError) {
        error = TRKReadBuffer1_ui8(buffer, &sp8);
    }

    if (error != kNoError) {
        TRKStandardACK(buffer, kDSReplyACK, kDSReplyError);
    } else if (sp9 == kDSOption_SerialIO) {
        SetUseSerialIO(sp8);
    }

    return TRKStandardACK(buffer, kDSReplyACK, kDSReplyNoError);
}
