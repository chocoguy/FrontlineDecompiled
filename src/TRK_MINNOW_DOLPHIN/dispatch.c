#include <TRK_MINNOW_DOLPHIN/trk.h>

typedef DSError (*TRKDispatchFunc)(TRKBuffer* buffer);

TRKDispatchFunc gTRKDispatchTable[33] = {
    TRKDoUnsupported,   TRKDoConnect,       TRKDoDisconnect,     TRKDoReset,          TRKDoVersions,
    TRKDoSupportMask,   TRKDoCPUType,       TRKDoUnsupported,    TRKDoUnsupported,    TRKDoUnsupported,
    TRKDoUnsupported,   TRKDoUnsupported,   TRKDoUnsupported,    TRKDoUnsupported,    TRKDoUnsupported,
    TRKDoUnsupported,   TRKDoReadMemory,    TRKDoWriteMemory,    TRKDoReadRegisters,  TRKDoWriteRegisters,
    TRKDoUnsupported,   TRKDoUnsupported,   TRKDoFlushCache,     TRKDoSetOption,      TRKDoContinue,
    TRKDoStep,          TRKDoStop,          TRKDoUnsupported,    TRKDoUnsupported,    TRKDoUnsupported,
    TRKDoUnsupported,   TRKDoUnsupported,   NULL,
};

u32 gTRKDispatchTableSize;

DSError TRKDispatchMessage(TRKBuffer* buffer) {
    DSError error;
    u8 command;

    error = kDispatchError;
    TRKSetBufferPosition(buffer, 0);
    TRKReadBuffer1_ui8(buffer, &command);
    if (command < gTRKDispatchTableSize) {
        error = gTRKDispatchTable[command](buffer);
    }
    return error;
}

DSError TRKInitializeDispatcher(void) {
    gTRKDispatchTableSize = 32;
    return kNoError;
}
