// Dolphin SDK AX (DSP audio) declarations. Layouts are this SDK version's, taken from the
// library code (__AXServiceVPB copies the parameter block field by field).
#ifndef _DOLPHIN_AX_H_
#define _DOLPHIN_AX_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AX_MAX_VOICES 64
#define AX_PRIORITY_STACKS 32
#define AX_PRIORITY_FREE 0

// Parameter block sync flags (AXVPB.sync): which pb fields to copy to the DSP.
#define AX_SYNC_FLAG_COPYSELECT      (1 << 0)
#define AX_SYNC_FLAG_COPYMXRCTRL     (1 << 1)
#define AX_SYNC_FLAG_COPYSTATE       (1 << 2)
#define AX_SYNC_FLAG_COPYTYPE        (1 << 3)
#define AX_SYNC_FLAG_COPYMIX         (1 << 4)
#define AX_SYNC_FLAG_COPYITD         (1 << 5)
#define AX_SYNC_FLAG_COPYTSHIFT      (1 << 6)
#define AX_SYNC_FLAG_COPYUPDATE      (1 << 7)
#define AX_SYNC_FLAG_COPYDPOP        (1 << 8)
#define AX_SYNC_FLAG_COPYVOL         (1 << 9)
#define AX_SYNC_FLAG_COPYDELTA       (1 << 10)
#define AX_SYNC_FLAG_COPYFIR         (1 << 11)
#define AX_SYNC_FLAG_COPYADDR        (1 << 12)
#define AX_SYNC_FLAG_COPYLOOP        (1 << 13)
#define AX_SYNC_FLAG_COPYLOOPADDR    (1 << 14)
#define AX_SYNC_FLAG_COPYENDADDR     (1 << 15)
#define AX_SYNC_FLAG_COPYCURADDR     (1 << 16)
#define AX_SYNC_FLAG_COPYADPCM       (1 << 17)
#define AX_SYNC_FLAG_COPYSRC         (1 << 18)
#define AX_SYNC_FLAG_COPYRATIO       (1 << 19)
#define AX_SYNC_FLAG_COPYADPCMLOOP   (1 << 20)
#define AX_SYNC_FLAG_COPYALL         (1 << 31)

#define AX_PB_STATE_STOP 0
#define AX_PB_STATE_RUN 1

typedef struct _AXPBMIX {
    u16 vL, vDeltaL;
    u16 vR, vDeltaR;
    u16 vAuxAL, vDeltaAuxAL;
    u16 vAuxAR, vDeltaAuxAR;
    u16 vAuxBL, vDeltaAuxBL;
    u16 vAuxBR, vDeltaAuxBR;
    u16 vAuxBS, vDeltaAuxBS;
    u16 vS, vDeltaS;
    u16 vAuxAS, vDeltaAuxAS;
} AXPBMIX;

typedef struct _AXPBITD {
    u16 flag;
    u16 bufferHi;
    u16 bufferLo;
    u16 shiftL;
    u16 shiftR;
    u16 targetShiftL;
    u16 targetShiftR;
} AXPBITD;

typedef struct _AXPBUPDATE {
    u16 updNum[5];
    u16 dataHi;
    u16 dataLo;
} AXPBUPDATE;

typedef struct _AXPBDPOP {
    u16 aL, aAuxAL, aAuxBL;
    u16 aR, aAuxAR, aAuxBR;
    u16 aS, aAuxAS, aAuxBS;
} AXPBDPOP;

typedef struct _AXPBVE {
    u16 currentVolume;
    s16 currentDelta;
} AXPBVE;

typedef struct _AXPBFIR {
    u16 numCoefs;
    u16 coefsHi;
    u16 coefsLo;
} AXPBFIR;

typedef struct _AXPBADDR {
    u16 loopFlag;
    u16 format;
    u16 loopAddressHi;
    u16 loopAddressLo;
    u16 endAddressHi;
    u16 endAddressLo;
    u16 currentAddressHi;
    u16 currentAddressLo;
} AXPBADDR;

typedef struct _AXPBADPCM {
    u16 a[8][2];
    u16 gain;
    u16 pred_scale;
    u16 yn1;
    u16 yn2;
} AXPBADPCM;

typedef struct _AXPBSRC {
    u16 ratioHi;
    u16 ratioLo;
    u16 currentAddressFrac;
    u16 last_samples[4];
} AXPBSRC;

typedef struct _AXPBADPCMLOOP {
    u16 loop_pred_scale;
    u16 loop_yn1;
    u16 loop_yn2;
} AXPBADPCMLOOP;

// The DSP's voice parameter block. size 0xC0
typedef struct _AXPB {
    /* 0x00 */ u16 nextHi;
    /* 0x02 */ u16 nextLo;
    /* 0x04 */ u16 currHi;
    /* 0x06 */ u16 currLo;
    /* 0x08 */ u16 srcSelect;
    /* 0x0A */ u16 coefSelect;
    /* 0x0C */ u16 mixerCtrl;
    /* 0x0E */ u16 state;
    /* 0x10 */ u16 type;
    /* 0x12 */ AXPBMIX mix;
    /* 0x36 */ AXPBITD itd;
    /* 0x44 */ AXPBUPDATE update;
    /* 0x52 */ AXPBDPOP dpop;
    /* 0x64 */ AXPBVE ve;
    /* 0x68 */ AXPBFIR fir;
    /* 0x6E */ AXPBADDR addr;
    /* 0x7E */ AXPBADPCM adpcm;
    /* 0xA6 */ AXPBSRC src;
    /* 0xB4 */ AXPBADPCMLOOP adpcmLoop;
    /* 0xBA */ u16 pad[3];
} AXPB;

struct _AXVPB;
typedef void (*AXVoiceCallback)(void* p);

// A voice. size 0x1F8
typedef struct _AXVPB {
    /* 0x000 */ void* next;
    /* 0x004 */ void* prev;
    /* 0x008 */ void* next1;
    /* 0x00C */ u32 priority;
    /* 0x010 */ AXVoiceCallback callback;
    /* 0x014 */ u32 userContext;
    /* 0x018 */ u32 index;
    /* 0x01C */ u32 sync;
    /* 0x020 */ u32 depop;
    /* 0x024 */ u32 updateMS;
    /* 0x028 */ u32 updateCounter;
    /* 0x02C */ u32 updateTotal;
    /* 0x030 */ u16* updateWrite;
    /* 0x034 */ u16 updateData[128];
    /* 0x134 */ void* itdBuffer;
    /* 0x138 */ AXPB pb;
} AXVPB;

typedef struct _AXSPB {
    u16 dpopLHi;
    u16 dpopLLo;
    s16 dpopLDelta;
    u16 dpopRHi;
    u16 dpopRLo;
    s16 dpopRDelta;
    u16 dpopSHi;
    u16 dpopSLo;
    s16 dpopSDelta;
    u16 dpopALHi;
    u16 dpopALLo;
    s16 dpopALDelta;
    u16 dpopARHi;
    u16 dpopARLo;
    s16 dpopARDelta;
    u16 dpopASHi;
    u16 dpopASLo;
    s16 dpopASDelta;
    u16 dpopBLHi;
    u16 dpopBLLo;
    s16 dpopBLDelta;
    u16 dpopBRHi;
    u16 dpopBRLo;
    s16 dpopBRDelta;
    u16 dpopBSHi;
    u16 dpopBSLo;
    s16 dpopBSDelta;
} AXSPB;

typedef struct _AXPROFILE {
    s64 axFrameStart;
    s64 auxProcessingStart;
    s64 auxProcessingEnd;
    s64 userCallbackStart;
    s64 userCallbackEnd;
    s64 axFrameEnd;
    s32 axNumVoices;
} AXPROFILE;

typedef void (*AXUserCallback)(void);
typedef void (*AXAuxCallback)(void* data, void* context);

#define AX_SRC_TYPE_NONE 0
#define AX_SRC_TYPE_LINEAR 1
#define AX_SRC_TYPE_4TAP_8K 2
#define AX_SRC_TYPE_4TAP_12K 3
#define AX_SRC_TYPE_4TAP_16K 4

#define AX_MODE_STEREO 0
#define AX_MODE_SURROUND 1
#define AX_MODE_DPL2 2

void AXInit(void);
void AXQuit(void);
AXVPB* AXAcquireVoice(u32 priority, AXVoiceCallback callback, u32 userContext);
void AXFreeVoice(AXVPB* p);
void AXSetVoiceSrcType(AXVPB* p, u32 type);
void AXSetVoiceState(AXVPB* p, u16 state);
void AXSetVoiceAddr(AXVPB* p, AXPBADDR* addr);
void AXSetVoiceAdpcm(AXVPB* p, AXPBADPCM* adpcm);
void AXSetVoiceSrcRatio(AXVPB* p, f32 ratio);
void AXSetVoiceAdpcmLoop(AXVPB* p, AXPBADPCMLOOP* adpcmloop);
void AXSetMode(u32 mode);
void AXRegisterAuxACallback(AXAuxCallback callback, void* context);
AXUserCallback AXRegisterCallback(AXUserCallback callback);

// Library internals.
AXVPB* __AXGetStackHead(u32 priority);
void __AXServiceCallbackStack(void);
void __AXAllocInit(void);
void __AXAllocQuit(void);
void __AXPushFreeStack(AXVPB* p);
void __AXPushCallbackStack(AXVPB* p);
AXVPB* __AXPopCallbackStack(void);
void __AXRemoveFromStack(AXVPB* p);
void __AXAuxInit(void);
void __AXAuxQuit(void);
void __AXGetAuxAInput(u32* addr);
void __AXGetAuxAOutput(u32* addr);
void __AXGetAuxBInput(u32* addr);
void __AXGetAuxBOutput(u32* addr);
void __AXProcessAux(void);
u32 __AXGetCommandListCycles(void);
u32 __AXGetCommandListAddress(void);
void __AXNextFrame(void* sbuffer, void* buffer);
void __AXClInit(void);
void __AXClQuit(void);
void __AXOutNewFrame(u32 lessDspCycles);
void __AXOutAiCallback(void);
void __AXOutInitDSP(void);
void __AXOutInit(void);
void __AXOutQuit(void);
AXSPB* __AXGetStudio(void);
void __AXSPBInit(void);
void __AXSPBQuit(void);
void __AXDepopVoice(AXPB* p);
u32 __AXGetNumVoices(void);
void __AXServiceVPB(AXVPB* pvpb);
void __AXSyncPBs(u32 lessDspCycles);
AXPB* __AXGetPBs(void);
void __AXSetPBDefault(AXVPB* p);
void __AXVPBInit(void);
void __AXVPBQuit(void);
AXPROFILE* __AXGetCurrentProfile(void);

extern u32 __AXClMode;

#ifdef __cplusplus
}
#endif

#endif
