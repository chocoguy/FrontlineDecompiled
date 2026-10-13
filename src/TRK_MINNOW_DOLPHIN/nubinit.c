#include <TRK_MINNOW_DOLPHIN/trk.h>

BOOL gTRKBigEndian;

static inline DSError TRKInitializeEndian(void) {
    u8 bendian[4];
    DSError result;

    result = FALSE;
    gTRKBigEndian = TRUE;

    bendian[0] = 0x12;
    bendian[1] = 0x34;
    bendian[2] = 0x56;
    bendian[3] = 0x78;

    if (*(u32*)bendian == 0x12345678) {
        gTRKBigEndian = TRUE;
    } else if (*(u32*)bendian == 0x78563412) {
        gTRKBigEndian = FALSE;
    } else {
        result = TRUE;
    }
    return result;
}

void TRKNubWelcome(void) {
    TRK_board_display("MetroTRK for GAMECUBE v0.10");
}

DSError TRKTerminateNub(void) {
    TRKTerminateSerialHandler();
    return kNoError;
}

DSError TRKInitializeNub(void) {
    DSError ret;
    DSError uartErr;

    ret = TRKInitializeEndian();

    if (ret == kNoError) {
        usr_put_initialize();
    }
    if (ret == kNoError) {
        ret = TRKInitializeEventQueue();
    }
    if (ret == kNoError) {
        ret = TRKInitializeMessageBuffers();
    }
    if (ret == kNoError) {
        ret = TRKInitializeDispatcher();
    }
    if (ret == kNoError) {
        uartErr = TRKInitializeIntDrivenUART(0x0000E100, 1, 0, (volatile u8**)&gTRKInputPendingPtr);
        TRKTargetSetInputPendingPtr(gTRKInputPendingPtr);
        if (uartErr != kNoError) {
            ret = uartErr;
        }
    }
    if (ret == kNoError) {
        ret = TRKInitializeSerialHandler();
    }
    if (ret == kNoError) {
        ret = TRKInitializeTarget();
    }

    return ret;
}
