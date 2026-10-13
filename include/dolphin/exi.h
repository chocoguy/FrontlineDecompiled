// Dolphin SDK EXI (expansion interface) declarations, following the public Dolphin SDK.
#ifndef _DOLPHIN_EXI_H_
#define _DOLPHIN_EXI_H_

#include "types.h"
#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXI_READ 0
#define EXI_WRITE 1
#define EXI_READWRITE 2

#define EXI_FREQ_1M 0
#define EXI_FREQ_2M 1
#define EXI_FREQ_4M 2
#define EXI_FREQ_8M 3
#define EXI_FREQ_16M 4
#define EXI_FREQ_32M 5

typedef void (*EXICallback)(s32 chan, OSContext* context);

BOOL EXIImm(s32 chan, void* buf, s32 len, u32 type, EXICallback callback);
BOOL EXIImmEx(s32 chan, void* buf, s32 len, u32 mode);
BOOL EXIDma(s32 chan, void* buf, s32 len, u32 type, EXICallback callback);
BOOL EXISync(s32 chan);
BOOL EXIProbe(s32 chan);
BOOL EXIAttach(s32 chan, EXICallback callback);
s32 EXIProbeEx(s32 chan);
s32 EXIGetID(s32 chan, u32 dev, u32* id);
u32 EXIClearInterrupts(s32 chan, BOOL exi, BOOL tc, BOOL ext);
void EXIInit(void);
u32 EXIGetState(s32 chan);
BOOL EXIDetach(s32 chan);
BOOL EXISelect(s32 chan, u32 dev, u32 freq);
BOOL EXIDeselect(s32 chan);
EXICallback EXISetExiCallback(s32 chan, EXICallback callback);
BOOL EXILock(s32 chan, u32 dev, EXICallback unlockedCallback);
BOOL EXIUnlock(s32 chan);

#ifdef __cplusplus
}
#endif

#endif
