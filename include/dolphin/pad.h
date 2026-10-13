// Dolphin SDK PAD (controller) declarations, added as decompiled code needs them.
#ifndef _DOLPHIN_PAD_H_
#define _DOLPHIN_PAD_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAD_ERR_NONE 0
#define PAD_ERR_NO_CONTROLLER -1
#define PAD_ERR_NOT_READY -2
#define PAD_ERR_TRANSFER -3

#define PAD_MOTOR_STOP 0
#define PAD_MOTOR_RUMBLE 1
#define PAD_MOTOR_STOP_HARD 2

typedef struct PADStatus {
    /* 0x0 */ u16 button;
    /* 0x2 */ s8 stickX;
    /* 0x3 */ s8 stickY;
    /* 0x4 */ s8 substickX;
    /* 0x5 */ s8 substickY;
    /* 0x6 */ u8 triggerLeft;
    /* 0x7 */ u8 triggerRight;
    /* 0x8 */ u8 analogA;
    /* 0x9 */ u8 analogB;
    /* 0xA */ s8 err;
} PADStatus;

BOOL PADReset(u32 mask);
u32 PADRead(PADStatus* status);  // status[4]
BOOL PADRecalibrate(u32 mask);
void PADControlMotor(s32 chan, u32 command);
void PADClamp(PADStatus* status);  // status[4]
typedef void (*PADSamplingCallback)(void);
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback);

#define PAD_CHAN0_BIT 0x80000000

BOOL __PADDisableRecalibration(BOOL disable);

#ifdef __cplusplus
}
#endif

#endif
