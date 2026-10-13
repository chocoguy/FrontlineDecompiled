#include <dolphin/ax.h>
#include <dolphin/os.h>

// Three rotating frames per bus: the DSP writes one, reads one, the CPU callback has the third.
// Each frame is 160 samples each of left, right and surround.
static s32 __AXBufferAuxA[3][480] __attribute__((aligned(32)));
static s32 __AXBufferAuxB[3][480] __attribute__((aligned(32)));

static AXAuxCallback __AXCallbackAuxA;
static AXAuxCallback __AXCallbackAuxB;
static void* __AXContextAuxA;
static void* __AXContextAuxB;
static s32* __AXAuxADspWrite;
static s32* __AXAuxADspRead;
static s32* __AXAuxBDspWrite;
static s32* __AXAuxBDspRead;
static u32 __AXAuxDspWritePosition;
static u32 __AXAuxDspReadPosition;
static u32 __AXAuxCpuReadWritePosition;

void __AXAuxInit(void) {
    u32 i;
    s32* pA;
    s32* pB;

    __AXCallbackAuxA = NULL;
    __AXCallbackAuxB = NULL;
    __AXContextAuxA = NULL;
    __AXContextAuxB = NULL;
    __AXAuxDspWritePosition = 0;
    __AXAuxDspReadPosition = 1;
    __AXAuxCpuReadWritePosition = 2;
    pA = __AXBufferAuxA[0];
    pB = __AXBufferAuxB[0];
    for (i = 0; i < 3 * 480; i++) {
        *pA++ = 0;
        *pB++ = 0;
    }
}

void __AXAuxQuit(void) {
    __AXCallbackAuxA = NULL;
    __AXCallbackAuxB = NULL;
}

void __AXGetAuxAInput(u32* addr) {
    if (__AXCallbackAuxA != NULL) {
        *addr = (u32)__AXBufferAuxA[__AXAuxDspWritePosition];
    } else {
        *addr = 0;
    }
}

void __AXGetAuxAOutput(u32* addr) {
    *addr = (u32)__AXBufferAuxA[__AXAuxDspReadPosition];
}

void __AXGetAuxBInput(u32* addr) {
    if (__AXCallbackAuxA != NULL) {
        *addr = (u32)__AXBufferAuxB[__AXAuxDspWritePosition];
    } else {
        *addr = 0;
    }
}

void __AXGetAuxBOutput(u32* addr) {
    *addr = (u32)__AXBufferAuxB[__AXAuxDspReadPosition];
}

void __AXProcessAux(void) {
    s32* data[3];

    __AXAuxADspWrite = __AXBufferAuxA[__AXAuxDspWritePosition];
    __AXAuxADspRead = __AXBufferAuxA[__AXAuxDspReadPosition];
    __AXAuxBDspWrite = __AXBufferAuxB[__AXAuxDspWritePosition];
    __AXAuxBDspRead = __AXBufferAuxB[__AXAuxDspReadPosition];
    if (__AXCallbackAuxA != NULL) {
        data[0] = __AXBufferAuxA[__AXAuxCpuReadWritePosition];
        data[1] = data[0] + 160;
        data[2] = data[0] + 320;
        DCInvalidateRange(data[0], 480 * 4);
        __AXCallbackAuxA(data, __AXContextAuxA);
        DCFlushRangeNoSync(data[0], 480 * 4);
    }
    if (__AXCallbackAuxB != NULL && __AXClMode != 4) {
        data[0] = __AXBufferAuxB[__AXAuxCpuReadWritePosition];
        data[1] = data[0] + 160;
        data[2] = data[0] + 320;
        DCInvalidateRange(data[0], 480 * 4);
        __AXCallbackAuxB(data, __AXContextAuxB);
        DCFlushRangeNoSync(data[0], 480 * 4);
    }
    __AXAuxDspWritePosition++;
    __AXAuxDspReadPosition++;
    __AXAuxCpuReadWritePosition++;
    __AXAuxDspWritePosition %= 3;
    __AXAuxDspReadPosition %= 3;
    __AXAuxCpuReadWritePosition %= 3;
}

void AXRegisterAuxACallback(AXAuxCallback callback, void* context) {
    __AXCallbackAuxA = callback;
    __AXContextAuxA = context;
}
