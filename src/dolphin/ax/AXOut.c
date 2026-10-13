#include <dolphin/ax.h>
#include <dolphin/ai.h>
#include <dolphin/dsp.h>
#include <dolphin/os.h>

extern u8 axDspSlave[];
extern u16 axDspSlaveLength;

static s16 __AXOutBuffer[2][320] __attribute__((aligned(32)));
static s32 __AXOutSBuffer[160] __attribute__((aligned(32)));
AXPROFILE __AXLocalProfile;
DSPTaskInfo task;
u16 ax_dram_image[0x2000] __attribute__((aligned(32)));

static u32 __AXOutFrame;
static volatile u32 __AXOutDspReady;  // 0: waiting for the DSP, 1: ready, 2: the AI got ahead
static OSTime __AXOsTime;
static AXUserCallback __AXUserFrameCallback;
static volatile BOOL __AXDSPInitFlag;
static volatile BOOL __AXDSPDoneFlag;
static BOOL __AXDebugSteppingMode;

// Runs the voices for the next frame and sends the command list to the DSP.
void __AXOutNewFrame(u32 lessDspCycles) {
    u32 cl;
    AXPROFILE* profile;

    __AXLocalProfile.axFrameStart = OSGetTime();
    __AXSyncPBs(lessDspCycles);
    __AXPrintStudio();
    cl = __AXGetCommandListAddress();
    DSPSendMailToDSP(0xBABE0180);
    while (DSPCheckMailToDSP()) {
    }
    DSPSendMailToDSP(cl);
    while (DSPCheckMailToDSP()) {
    }
    __AXServiceCallbackStack();
    __AXLocalProfile.auxProcessingStart = OSGetTime();
    __AXProcessAux();
    __AXLocalProfile.auxProcessingEnd = OSGetTime();
    __AXLocalProfile.userCallbackStart = OSGetTime();
    if (__AXUserFrameCallback != NULL) {
        __AXUserFrameCallback();
    }
    __AXLocalProfile.userCallbackEnd = OSGetTime();
    __AXNextFrame(__AXOutSBuffer, __AXOutBuffer[__AXOutFrame]);
    __AXOutFrame++;
    __AXOutFrame &= 1;
    AIInitDMA((u32)__AXOutBuffer[__AXOutFrame], sizeof(__AXOutBuffer[0]));
    __AXLocalProfile.axFrameEnd = OSGetTime();
    __AXLocalProfile.axNumVoices = __AXGetNumVoices();
    profile = __AXGetCurrentProfile();
    if (profile != NULL) {
        u8* src = (u8*)&__AXLocalProfile;
        u8* dst = (u8*)profile;
        u32 i;
        for (i = 0; i < sizeof(AXPROFILE); i++) {
            *dst++ = *src++;
        }
    }
}

void __AXOutAiCallback(void) {
    if (__AXOutDspReady == 0) {
        __AXOsTime = OSGetTime();
    }
    if (__AXOutDspReady == 1) {
        __AXOutDspReady = 0;
        __AXOutNewFrame(0);
    } else {
        __AXOutDspReady = 2;
        DSPAssertTask(&task);
    }
}

static void __AXDSPInitCallback(void* task) {
    __AXDSPInitFlag = TRUE;
}

// The DSP finished the frame: start the next one now if the AI is already waiting.
static void __AXDSPResumeCallback(void* task) {
    if (__AXOutDspReady == 2) {
        __AXOutDspReady = 0;
        __AXOutNewFrame((u32)(OSGetTime() - __AXOsTime) / 4);
    } else {
        __AXOutDspReady = 1;
    }
}

static void __AXDSPDoneCallback(void* task) {
    __AXDSPDoneFlag = TRUE;
}

void __AXOutInitDSP(void) {
    task.iram_mmem_addr = (u16*)axDspSlave;
    task.iram_length = axDspSlaveLength;
    task.iram_addr = 0;
    task.dram_mmem_addr = ax_dram_image;
    task.dram_length = 0x2000;
    task.dram_addr = 0;
    task.dsp_init_vector = 0x10;
    task.dsp_resume_vector = 0x30;
    task.init_cb = __AXDSPInitCallback;
    task.res_cb = __AXDSPResumeCallback;
    task.done_cb = __AXDSPDoneCallback;
    task.req_cb = NULL;
    task.priority = 0;
    __AXDSPInitFlag = FALSE;
    __AXDSPDoneFlag = FALSE;
    if (!DSPCheckInit()) {
        DSPInit();
    }
    DSPAddTask(&task);
    while (!__AXDSPInitFlag) {
    }
}

void __AXOutInit(void) {
    u32 i;
    u32* p;

    __AXOutFrame = 0;
    __AXDebugSteppingMode = FALSE;
    p = (u32*)__AXOutBuffer;
    for (i = 0; i < sizeof(__AXOutBuffer) / 4; i++) {
        *p++ = 0;
    }
    DCFlushRange(__AXOutBuffer, sizeof(__AXOutBuffer));
    p = (u32*)__AXOutSBuffer;
    for (i = 0; i < sizeof(__AXOutSBuffer) / 4; i++) {
        *p++ = 0;
    }
    DCFlushRange(__AXOutSBuffer, sizeof(__AXOutSBuffer));
    __AXOutInitDSP();
    AIRegisterDMACallback(__AXOutAiCallback);
    __AXNextFrame(__AXOutSBuffer, __AXOutBuffer[1]);
    __AXOutDspReady = 1;
    __AXUserFrameCallback = NULL;
    AIInitDMA((u32)__AXOutBuffer[__AXOutFrame], sizeof(__AXOutBuffer[0]));
    AIStartDMA();
}

void __AXOutQuit(void) {
    BOOL old = OSDisableInterrupts();
    __AXUserFrameCallback = NULL;
    AIStopDMA();
    DSPHalt();
    while (DSPGetDMAStatus()) {
    }
    DSPReset();
    OSRestoreInterrupts(old);
}
