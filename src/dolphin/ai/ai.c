#include <dolphin/ai.h>
#include <dolphin/os.h>

#define __DSPRegs ((volatile u16*)0xCC005000)
#define __AIRegs ((volatile u32*)0xCC006C00)

#define OS_TIMER_CLOCK_ (OS_BUS_CLOCK / 4)
#define OSNanosecondsToTicks_(nsec) (((nsec) * (OS_TIMER_CLOCK_ / 125000)) / 8000)

static AISCallback __AIS_Callback;
static AIDCallback __AID_Callback;
static u8* __CallbackStack;
static u8* __OldStack;
static volatile BOOL __AI_init_flag;
static OSTime bound_32KHz;
static OSTime bound_48KHz;
static OSTime min_wait;
static OSTime max_wait;
static OSTime buffer;

static void __AI_set_stream_sample_rate(u32 rate);
static void __AISHandler(__OSInterrupt interrupt, OSContext* context);
static void __AIDHandler(__OSInterrupt interrupt, OSContext* context);
static void __AICallbackStackSwitch(register void* cb);
static void __AI_SRC_INIT(void);

AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    AIDCallback old = __AID_Callback;
    BOOL enabled = OSDisableInterrupts();
    __AID_Callback = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

void AIInitDMA(u32 start_addr, u32 length) {
    BOOL old = OSDisableInterrupts();
    __DSPRegs[24] = (u16)((__DSPRegs[24] & ~0x3FF) | (start_addr >> 16));
    __DSPRegs[25] = (u16)((__DSPRegs[25] & ~0xFFE0) | (0xFFFF & start_addr));
    __DSPRegs[27] = (u16)((__DSPRegs[27] & ~0x7FFF) | (u16)((length >> 5) & 0xFFFF));
    OSRestoreInterrupts(old);
}

void AIStartDMA(void) {
    __DSPRegs[27] |= 0x8000;
}

void AIStopDMA(void) {
    __DSPRegs[27] &= ~0x8000;
}

void AISetStreamPlayState(u32 state) {
    BOOL old;
    u8 volRight;
    u8 volLeft;
    if (state == AIGetStreamPlayState()) {
        return;
    }
    if (AIGetStreamSampleRate() == AI_SAMPLERATE_32KHZ && state == AI_STREAM_START) {
        volRight = AIGetStreamVolRight();
        volLeft = AIGetStreamVolLeft();
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        old = OSDisableInterrupts();
        __AI_SRC_INIT();
        __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
        __AIRegs[0] = (__AIRegs[0] & ~1) | 1;
        OSRestoreInterrupts(old);
        AISetStreamVolLeft(volRight);
        AISetStreamVolRight(volLeft);
    } else {
        __AIRegs[0] = (__AIRegs[0] & ~1) | state;
    }
}

u32 AIGetStreamPlayState(void) {
    return __AIRegs[0] & 1;
}

void AISetDSPSampleRate(u32 rate) {
    u32 state;
    BOOL old;
    u8 volLeft;
    u8 volRight;
    u32 sampleRate;
    if (rate == AIGetDSPSampleRate()) {
        return;
    }
    __AIRegs[0] &= ~0x40;
    if (rate == AI_SAMPLERATE_32KHZ) {
        volLeft = AIGetStreamVolLeft();
        volRight = AIGetStreamVolRight();
        state = AIGetStreamPlayState();
        sampleRate = AIGetStreamSampleRate();
        AISetStreamVolLeft(0);
        AISetStreamVolRight(0);
        old = OSDisableInterrupts();
        __AI_SRC_INIT();
        __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
        __AIRegs[0] = (__AIRegs[0] & ~2) | (sampleRate * 2);
        __AIRegs[0] = (__AIRegs[0] & ~1) | state;
        __AIRegs[0] |= 0x40;
        OSRestoreInterrupts(old);
        AISetStreamVolLeft(volLeft);
        AISetStreamVolRight(volRight);
    }
}

u32 AIGetDSPSampleRate(void) {
    return ((__AIRegs[0] >> 6) & 1) ^ 1;
}

static void __AI_set_stream_sample_rate(u32 rate) {
    BOOL old;
    u32 state;
    u8 volLeft;
    u8 volRight;
    u32 dspRate;
    if (rate == AIGetStreamSampleRate()) {
        return;
    }
    state = AIGetStreamPlayState();
    volLeft = AIGetStreamVolLeft();
    volRight = AIGetStreamVolRight();
    AISetStreamVolRight(0);
    AISetStreamVolLeft(0);
    dspRate = __AIRegs[0] & 0x40;
    __AIRegs[0] &= ~0x40;
    old = OSDisableInterrupts();
    __AI_SRC_INIT();
    __AIRegs[0] |= dspRate;
    __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
    __AIRegs[0] = (__AIRegs[0] & ~2) | (rate * 2);
    OSRestoreInterrupts(old);
    AISetStreamPlayState(state);
    AISetStreamVolLeft(volLeft);
    AISetStreamVolRight(volRight);
}

u32 AIGetStreamSampleRate(void) {
    return (__AIRegs[0] >> 1) & 1;
}

void AISetStreamVolLeft(u8 volume) {
    __AIRegs[1] = (__AIRegs[1] & ~0xFF) | (volume & 0xFF);
}

u8 AIGetStreamVolLeft(void) {
    return (u8)__AIRegs[1];
}

void AISetStreamVolRight(u8 volume) {
    __AIRegs[1] = (__AIRegs[1] & ~0xFF00) | ((volume & 0xFF) << 8);
}

u8 AIGetStreamVolRight(void) {
    return (u8)(__AIRegs[1] >> 8);
}

void AIInit(u8* stack) {
    if (__AI_init_flag == TRUE) {
        return;
    }
    bound_32KHz = OSNanosecondsToTicks_(31524);
    bound_48KHz = OSNanosecondsToTicks_(42024);
    min_wait = OSNanosecondsToTicks_(42000);
    max_wait = OSNanosecondsToTicks_(63000);
    buffer = OSNanosecondsToTicks_(3000);
    AISetStreamVolRight(0);
    AISetStreamVolLeft(0);
    __AIRegs[3] = 0;
    __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
    __AI_set_stream_sample_rate(AI_SAMPLERATE_48KHZ);
    AISetDSPSampleRate(AI_SAMPLERATE_32KHZ);
    __AIS_Callback = NULL;
    __AID_Callback = NULL;
    __CallbackStack = stack;
    __OSSetInterruptHandler(5, __AIDHandler);
    __OSUnmaskInterrupts(0x04000000);
    __OSSetInterruptHandler(8, __AISHandler);
    __OSUnmaskInterrupts(0x00800000);
    __AI_init_flag = TRUE;
}

static void __AISHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    __AIRegs[0] |= 8;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AIS_Callback != NULL) {
        __AIS_Callback(__AIRegs[2]);
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void __AIDHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    u16 tmp = __DSPRegs[5];
    tmp = (u16)((tmp & ~0xA0) | 8);
    __DSPRegs[5] = tmp;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AID_Callback != NULL) {
        if (__CallbackStack != NULL) {
            __AICallbackStackSwitch(__AID_Callback);
        } else {
            __AID_Callback();
        }
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

// Runs the DMA callback on the callback stack.
static asm void __AICallbackStackSwitch(register void* cb) {
    fralloc
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    stw r1, 0(r5)
    lis r5, __CallbackStack@ha
    addi r5, r5, __CallbackStack@l
    lwz r1, 0(r5)
    subi r1, r1, 8
    mtlr cb
    blrl
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    lwz r1, 0(r5)
    frfree
    blr
}

// Waits out the stream sample counter's phase after a rate change.
static void __AI_SRC_INIT(void) {
    OSTime rise32 = 0;
    OSTime rise48 = 0;
    OSTime diff = 0;
    OSTime temp = 0;
    u32 temp0 = 0;
    u32 temp1 = 0;
    u32 done = 0;
    u32 walking = 0;
    u32 initCnt = 0;

    while (!done) {
        __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
        __AIRegs[0] &= ~2;
        __AIRegs[0] = (__AIRegs[0] & ~1) | 1;
        temp0 = __AIRegs[2];
        while (temp0 == __AIRegs[2]) {
        }
        rise32 = OSGetTime();
        __AIRegs[0] = (__AIRegs[0] & ~2) | 2;
        __AIRegs[0] = (__AIRegs[0] & ~1) | 1;
        temp1 = __AIRegs[2];
        while (temp1 == __AIRegs[2]) {
        }
        rise48 = OSGetTime();
        diff = rise48 - rise32;
        __AIRegs[0] &= ~2;
        __AIRegs[0] &= ~1;
        if (diff < (bound_32KHz - buffer)) {
            temp = min_wait;
            done = 1;
            initCnt++;
        } else if (diff >= (bound_32KHz + buffer) && diff < (bound_48KHz - buffer)) {
            temp = max_wait;
            done = 1;
            initCnt++;
        } else {
            done = 0;
            walking = 1;
            initCnt++;
        }
    }
    while ((rise48 + temp) > OSGetTime()) {
    }
}
