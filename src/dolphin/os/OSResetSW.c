#include <dolphin/os.h>
#include <dolphin/os_priv.h>

#define __PIRegs ((volatile u32*)0xCC003000)

#define PI_INTSR_RSWST 0x00010000  // set while the reset button is up
#define OS_INTERRUPTMASK_PI_RSW 0x00000200

// Bits 0-5: minutes until the reset button is faked (kiosk demo builds).
#define GameChoice (*(volatile u8*)0x800030E3)

static OSResetCallback ResetCallback;
static BOOL Down;
static BOOL LastState;
static OSTime HoldUp;
static OSTime HoldDown;

void __OSResetSWInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    OSResetCallback callback;

    // Debounce for 100us.
    HoldDown = __OSGetSystemTime();
    while (__OSGetSystemTime() - HoldDown < OSMicrosecondsToTicks(100) && !(__PIRegs[0] & PI_INTSR_RSWST)) {
    }

    if (!(__PIRegs[0] & PI_INTSR_RSWST)) {
        LastState = Down = TRUE;
        __OSMaskInterrupts(OS_INTERRUPTMASK_PI_RSW);
        if (ResetCallback) {
            callback = ResetCallback;
            ResetCallback = NULL;
            callback();
        }
    }
    __PIRegs[0] = 2;
}

u32 OSGetResetButtonState(void) {
    BOOL enabled;
    BOOL state;
    u32 reg;
    OSTime now;

    enabled = OSDisableInterrupts();

    now = __OSGetSystemTime();

    reg = __PIRegs[0];
    if (!(reg & PI_INTSR_RSWST)) {
        if (!Down) {
            Down = TRUE;
            state = HoldUp ? TRUE : FALSE;
            HoldDown = now;
        } else {
            state = (HoldUp || (OSMicrosecondsToTicks(100) < now - HoldDown)) ? TRUE : FALSE;
        }
    } else if (Down) {
        Down = FALSE;
        state = LastState;
        if (state) {
            HoldUp = now;
        } else {
            HoldUp = 0;
        }
    } else if (HoldUp && (now - HoldUp < OSMillisecondsToTicks(40))) {
        state = TRUE;
    } else {
        state = FALSE;
        HoldUp = 0;
    }

    LastState = state;

    if (GameChoice & 0x3F) {
        OSTime fire = (GameChoice & 0x3F) * 60;

        fire = __OSStartTime + OSSecondsToTicks(fire);
        if (fire < now) {
            now -= fire;
            now = OSTicksToSeconds(now) / 2;
            if ((now & 1) == 0) {
                state = TRUE;
            } else {
                state = FALSE;
            }
        }
    }

    OSRestoreInterrupts(enabled);
    return state;
}
