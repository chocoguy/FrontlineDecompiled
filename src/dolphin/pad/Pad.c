#include <dolphin/pad.h>
#include <dolphin/si.h>
#include <string.h>

// Byte 3 of the boot info's game choice; bit 6 disables recalibration on reset.
#define GameChoice (*(volatile u8*)0x800030E3)

#define PAD_BUTTON_ALL 0x3FFF
#define PAD_ERR_STATUS_SIZE 0xA  // offsetof(PADStatus, err)
#define SI_ERROR_FATAL (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION)

static u32 Type[SI_MAX_CHAN];
static PADStatus Origin[SI_MAX_CHAN];
static u32 CmdProbeDevice[SI_MAX_CHAN];

static s32 ResettingChan = 32;
static u32 XPatchBits = PAD_CHAN0_BIT | 0x40000000 | 0x20000000 | 0x10000000;
static u32 AnalogMode = 0x00000300;
static u32 Spec = 5;  // PAD_SPEC_5
static void SPEC2_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static void (*MakeStatus)(s32, PADStatus*, u32*) = SPEC2_MakeStatus;
static u32 CmdReadOrigin = 0x41 << 24;
static u32 CmdCalibrate = 0x42 << 24;

static u32 EnabledBits;
static u32 ResettingBits;
static u32 RecalibrateBits;
static u32 WaitingBits;
static u32 CheckingBits;
static u32 PendingBits;
static PADSamplingCallback SamplingCallback;
u32 __PADSpec;

static void PADTypeAndStatusCallback(s32 chan, u32 type);

// Starts resetting the next channel in ResettingBits (32 = none).
static inline void DoReset(void) {
    u32 chanBit;

    ResettingChan = __cntlzw(ResettingBits);
    if (ResettingChan == 32) {
        return;
    }

    chanBit = PAD_CHAN0_BIT >> ResettingChan;
    ResettingBits &= ~chanBit;

    memset(&Origin[ResettingChan], 0, sizeof(PADStatus));
    SIGetTypeAsync(ResettingChan, PADTypeAndStatusCallback);
}

static inline void PADEnable(s32 chan) {
    u32 cmd;
    u32 chanBit;
    u32 data[2];

    chanBit = PAD_CHAN0_BIT >> chan;
    EnabledBits |= chanBit;
    SIGetResponse(chan, data);
    cmd = (0x40 << 16) | AnalogMode;
    SISetCommand(chan, cmd);
    SIEnablePolling(EnabledBits);
}

static inline void PADDisable(s32 chan) {
    BOOL enabled;
    u32 chanBit;

    enabled = OSDisableInterrupts();

    chanBit = PAD_CHAN0_BIT >> chan;
    SIDisablePolling(chanBit);
    EnabledBits &= ~chanBit;
    WaitingBits &= ~chanBit;
    CheckingBits &= ~chanBit;
    PendingBits &= ~chanBit;
    OSSetWirelessID(chan, 0);

    OSRestoreInterrupts(enabled);
}

static void UpdateOrigin(s32 chan) {
    PADStatus* origin;
    u32 chanBit = PAD_CHAN0_BIT >> chan;

    origin = &Origin[chan];
    switch (AnalogMode & 0x00000700u) {
    case 0x00000000u:
    case 0x00000500u:
    case 0x00000600u:
    case 0x00000700u:
        origin->triggerLeft &= ~15;
        origin->triggerRight &= ~15;
        origin->analogA &= ~15;
        origin->analogB &= ~15;
        break;
    case 0x00000100u:
        origin->substickX &= ~15;
        origin->substickY &= ~15;
        origin->analogA &= ~15;
        origin->analogB &= ~15;
        break;
    case 0x00000200u:
        origin->substickX &= ~15;
        origin->substickY &= ~15;
        origin->triggerLeft &= ~15;
        origin->triggerRight &= ~15;
        break;
    case 0x00000300u:
        break;
    case 0x00000400u:
        break;
    }

    origin->stickX -= 128;
    origin->stickY -= 128;
    origin->substickX -= 128;
    origin->substickY -= 128;

    // Early standard controllers report a bad X origin.
    if (XPatchBits & chanBit) {
        if (64 < origin->stickX && (SIGetType(chan) & 0xFFFF0000) == SI_GC_CONTROLLER) {
            origin->stickX = 0;
        }
    }
}

static void PADOriginCallback(s32 chan, u32 error, OSContext* context) {
    if (!(error & SI_ERROR_FATAL)) {
        UpdateOrigin(ResettingChan);
        PADEnable(ResettingChan);
    }
    DoReset();
}

static void PADOriginUpdateCallback(s32 chan, u32 error, OSContext* context) {
    if (!(EnabledBits & (PAD_CHAN0_BIT >> chan))) {
        return;
    }

    if (!(error & SI_ERROR_FATAL)) {
        UpdateOrigin(chan);
    }

    if (error & SI_ERROR_NO_RESPONSE) {
        PADDisable(chan);
    }
}

static void PADProbeCallback(s32 chan, u32 error, OSContext* context) {
    if (!(error & SI_ERROR_FATAL)) {
        PADEnable(ResettingChan);
        WaitingBits |= PAD_CHAN0_BIT >> ResettingChan;
    }
    DoReset();
}

static void PADTypeAndStatusCallback(s32 chan, u32 type) {
    u32 chanBit;
    u32 recalibrate;
    BOOL rc = TRUE;
    u32 error;

    chanBit = PAD_CHAN0_BIT >> ResettingChan;
    error = type & 0xFF;
    recalibrate = RecalibrateBits & chanBit;
    RecalibrateBits &= ~chanBit;

    if (error & SI_ERROR_FATAL) {
        DoReset();
        return;
    }

    type &= ~0xFF;
    Type[ResettingChan] = type;

    if ((type & SI_TYPE_MASK) != SI_TYPE_GC || !(type & SI_GC_STANDARD)) {
        DoReset();
        return;
    }

    if (Spec < 2) {  // PAD_SPEC_2
        PADEnable(ResettingChan);
        DoReset();
        return;
    }

    if (!(type & SI_GC_WIRELESS) || (type & SI_WIRELESS_IR)) {
        if (recalibrate) {
            rc = SITransfer(ResettingChan, &CmdCalibrate, 3, &Origin[ResettingChan], 10, PADOriginCallback, 0);
        } else {
            rc = SITransfer(ResettingChan, &CmdReadOrigin, 1, &Origin[ResettingChan], 10, PADOriginCallback, 0);
        }
    } else if ((type & SI_WIRELESS_FIX_ID) && !(type & SI_WIRELESS_CONT_MASK) && !(type & SI_WIRELESS_LITE)) {
        if (type & SI_WIRELESS_RECEIVED) {
            rc = SITransfer(ResettingChan, &CmdReadOrigin, 1, &Origin[ResettingChan], 10, PADOriginCallback, 0);
        } else {
            rc = SITransfer(ResettingChan, &CmdProbeDevice[ResettingChan], 3, &Origin[ResettingChan], 8,
                            PADProbeCallback, 0);
        }
    }
    if (!rc) {
        PendingBits |= chanBit;
        DoReset();
        return;
    }
}

static void PADReceiveCheckCallback(s32 chan, u32 type) {
    u32 error;
    u32 chanBit;

    chanBit = PAD_CHAN0_BIT >> chan;
    if (!(EnabledBits & chanBit)) {
        return;
    }

    error = type & 0xFF;
    type &= ~0xFF;

    WaitingBits &= ~chanBit;
    CheckingBits &= ~chanBit;

    if (!(error & SI_ERROR_FATAL) && (type & SI_GC_WIRELESS) && (type & SI_WIRELESS_FIX_ID) &&
        (type & SI_WIRELESS_RECEIVED) && !(type & SI_WIRELESS_IR) && !(type & SI_WIRELESS_CONT_MASK) &&
        !(type & SI_WIRELESS_LITE)) {
        SITransfer(chan, &CmdReadOrigin, 1, &Origin[chan], 10, PADOriginUpdateCallback, 0);
    } else {
        PADDisable(chan);
    }
}

BOOL PADReset(u32 mask) {
    BOOL enabled;
    u32 disableBits;

    enabled = OSDisableInterrupts();

    mask |= PendingBits;
    PendingBits = 0;
    mask &= ~(WaitingBits | CheckingBits);
    ResettingBits |= mask;
    disableBits = ResettingBits & EnabledBits;
    EnabledBits &= ~mask;

    if (Spec == 4) {  // PAD_SPEC_4
        RecalibrateBits |= mask;
    }

    SIDisablePolling(disableBits);

    if (ResettingChan == 32) {
        DoReset();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL PADRecalibrate(u32 mask) {
    BOOL enabled;
    u32 disableBits;

    enabled = OSDisableInterrupts();

    mask |= PendingBits;
    PendingBits = 0;
    mask &= ~(WaitingBits | CheckingBits);
    ResettingBits |= mask;
    disableBits = ResettingBits & EnabledBits;
    EnabledBits &= ~mask;

    if (!(GameChoice & 0x40)) {
        RecalibrateBits |= mask;
    }

    SIDisablePolling(disableBits);
    if (ResettingChan == 32) {
        DoReset();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

u32 PADRead(PADStatus* status) {
    BOOL enabled;
    s32 chan;
    u32 data[2];
    u32 chanBit;
    u32 sr;
    u32 motor;

    enabled = OSDisableInterrupts();

    motor = 0;
    for (chan = 0; chan < SI_MAX_CHAN; chan++, status++) {
        chanBit = PAD_CHAN0_BIT >> chan;

        if (PendingBits & chanBit) {
            PADReset(0);
            status->err = PAD_ERR_NOT_READY;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        if ((ResettingBits & chanBit) || ResettingChan == chan) {
            status->err = PAD_ERR_NOT_READY;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        if (!(EnabledBits & chanBit)) {
            status->err = (s8)PAD_ERR_NO_CONTROLLER;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        if (SIIsChanBusy(chan)) {
            status->err = PAD_ERR_TRANSFER;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        sr = SIGetStatus(chan);
        if (sr & SI_ERROR_NO_RESPONSE) {
            SIGetResponse(chan, data);

            if (WaitingBits & chanBit) {
                status->err = (s8)PAD_ERR_NONE;
                memset(status, 0, PAD_ERR_STATUS_SIZE);

                if (!(CheckingBits & chanBit)) {
                    CheckingBits |= chanBit;
                    SIGetTypeAsync(chan, PADReceiveCheckCallback);
                }
                continue;
            }

            PADDisable(chan);

            status->err = (s8)PAD_ERR_NO_CONTROLLER;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        if (!(SIGetType(chan) & SI_GC_NOMOTOR)) {
            motor |= chanBit;
        }

        if (!SIGetResponse(chan, data)) {
            status->err = PAD_ERR_TRANSFER;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        if (data[0] & 0x80000000) {
            status->err = PAD_ERR_TRANSFER;
            memset(status, 0, PAD_ERR_STATUS_SIZE);
            continue;
        }

        MakeStatus(chan, status, data);

        // Origin changed (X+Y+Start held): read the new origin.
        if (status->button & 0x2000) {
            status->err = PAD_ERR_TRANSFER;
            memset(status, 0, PAD_ERR_STATUS_SIZE);

            SITransfer(chan, &CmdReadOrigin, 1, &Origin[chan], 10, PADOriginUpdateCallback, 0);
            continue;
        }

        status->err = PAD_ERR_NONE;
        status->button &= ~0x0080;
    }

    OSRestoreInterrupts(enabled);
    return motor;
}

void PADControlMotor(s32 chan, u32 command) {
    BOOL enabled;
    u32 chanBit;

    enabled = OSDisableInterrupts();
    chanBit = PAD_CHAN0_BIT >> chan;
    if ((EnabledBits & chanBit) && !(SIGetType(chan) & SI_GC_NOMOTOR)) {
        if (Spec < 2 && command == PAD_MOTOR_STOP_HARD) {
            command = PAD_MOTOR_STOP;
        }
        SISetCommand(chan, (0x40 << 16) | AnalogMode | (command & (0x00000001 | 0x00000002)));
        SITransferCommands();
    }
    OSRestoreInterrupts(enabled);
}

static s8 ClampS8(s8 var, s8 org) {
    if (0 < org) {
        s8 min = (s8)(-128 + org);
        if (var < min) {
            var = min;
        }
    } else if (org < 0) {
        s8 max = (s8)(127 + org);
        if (max < var) {
            var = max;
        }
    }
    return var -= org;
}

static u8 ClampU8(u8 var, u8 org) {
    if (var < org) {
        var = org;
    }
    return var -= org;
}

static void SPEC2_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    PADStatus* origin;

    status->button = (u16)((data[0] >> 16) & PAD_BUTTON_ALL);
    status->stickX = (s8)(data[0] >> 8);
    status->stickY = (s8)(data[0]);

    switch (AnalogMode & 0x00000700) {
    case 0x00000000:
    case 0x00000500:
    case 0x00000600:
    case 0x00000700:
        status->substickX = (s8)(data[1] >> 24);
        status->substickY = (s8)(data[1] >> 16);
        status->triggerLeft = (u8)((data[1] >> 8) & 0xF0);
        status->triggerRight = (u8)((data[1] >> 4) & 0xF0);
        status->analogA = (u8)((data[1] >> 0) & 0xF0);
        status->analogB = (u8)((data[1] << 4) & 0xF0);
        break;
    case 0x00000100:
        status->substickX = (s8)((data[1] >> 24) & 0xF0);
        status->substickY = (s8)((data[1] >> 20) & 0xF0);
        status->triggerLeft = (u8)((data[1] >> 16) & 0xFF);
        status->triggerRight = (u8)((data[1] >> 8) & 0xFF);
        status->analogA = (u8)((data[1] >> 0) & 0xF0);
        status->analogB = (u8)((data[1] << 4) & 0xF0);
        break;
    case 0x00000200:
        status->substickX = (s8)((data[1] >> 24) & 0xF0);
        status->substickY = (s8)((data[1] >> 20) & 0xF0);
        status->triggerLeft = (u8)((data[1] >> 16) & 0xF0);
        status->triggerRight = (u8)((data[1] >> 12) & 0xF0);
        status->analogA = (u8)((data[1] >> 8) & 0xFF);
        status->analogB = (s8)((data[1] >> 0) & 0xFF);
        break;
    case 0x00000300:
        status->substickX = (s8)(data[1] >> 24);
        status->substickY = (s8)(data[1] >> 16);
        status->triggerLeft = (u8)(data[1] >> 8);
        status->triggerRight = (u8)(data[1] >> 0);
        status->analogA = 0;
        status->analogB = 0;
        break;
    case 0x00000400:
        status->substickX = (s8)(data[1] >> 24);
        status->substickY = (s8)(data[1] >> 16);
        status->triggerLeft = 0;
        status->triggerRight = 0;
        status->analogA = (u8)(data[1] >> 8);
        status->analogB = (u8)(data[1] >> 0);
        break;
    }

    status->stickX -= 128;
    status->stickY -= 128;
    status->substickX -= 128;
    status->substickY -= 128;

    origin = &Origin[chan];
    status->stickX = ClampS8(status->stickX, origin->stickX);
    status->stickY = ClampS8(status->stickY, origin->stickY);
    status->substickX = ClampS8(status->substickX, origin->substickX);
    status->substickY = ClampS8(status->substickY, origin->substickY);
    status->triggerLeft = ClampU8(status->triggerLeft, origin->triggerLeft);
    status->triggerRight = ClampU8(status->triggerRight, origin->triggerRight);
}

static void SamplingHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;

    if (SamplingCallback) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        SamplingCallback();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) {
    PADSamplingCallback prev;

    prev = SamplingCallback;
    SamplingCallback = callback;
    if (callback) {
        SIRegisterPollingHandler(SamplingHandler);
    } else {
        SIUnregisterPollingHandler(SamplingHandler);
    }
    return prev;
}

BOOL __PADDisableRecalibration(BOOL disable) {
    BOOL enabled;
    BOOL prev;

    enabled = OSDisableInterrupts();
    prev = (GameChoice & 0x40) ? TRUE : FALSE;
    GameChoice &= (u8)~0x40;
    if (disable) {
        GameChoice |= 0x40;
    }
    OSRestoreInterrupts(enabled);
    return prev;
}
