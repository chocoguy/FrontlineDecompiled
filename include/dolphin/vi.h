// Dolphin SDK VI (video interface) declarations, added as decompiled code needs them.
#ifndef _DOLPHIN_VI_H_
#define _DOLPHIN_VI_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*VIRetraceCallback)(u32 retraceCount);

void VIInit(void);
u32 VIGetCurrentLine(void);
u32 VIGetTvFormat(void);  // 0 NTSC, 1 PAL, 2 MPAL
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback);
void VIConfigure(const struct _GXRenderModeObj* rm);
void VISetNextFrameBuffer(void* fb);
u32 VIGetNextField(void);
void VISetBlack(BOOL black);
void VIFlush(void);
void VIWaitForRetrace(void);

#ifdef __cplusplus
}
#endif

#endif
