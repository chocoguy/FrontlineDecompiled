#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>
#include <dolphin/os_priv.h>

#define GX_BM_NONE 0
#define GX_BM_BLEND 1
#define GX_BM_LOGIC 2
#define GX_BM_SUBTRACT 3

static GXDrawSyncCallback TokenCB;
static GXDrawDoneCallback DrawDoneCB;
static volatile u8 DrawDone;
static OSThreadQueue FinishQueue;

// Kept out of line as in the original.
#pragma auto_inline off
void GXFlush(void) {
    u32 i;

    if (gx->dirtyState) {
        __GXSetDirtyState();
    }

    // Push 32 bytes through the write-gather pipe so everything queued reaches the FIFO.
    for (i = 32; i > 0; i -= 4) {
        GX_WRITE_U32(0);
    }

    PPCSync();
}
#pragma auto_inline reset

void GXDrawDone(void) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    GX_WRITE_RAS_REG(0x45000002);  // PE_DONE: raises the finish interrupt
    GXFlush();
    DrawDone = 0;
    OSRestoreInterrupts(enabled);

    enabled = OSDisableInterrupts();
    while (!DrawDone) {
        OSSleepThread(&FinishQueue);
    }
    OSRestoreInterrupts(enabled);
}

void GXPokeAlphaMode(u32 func, u8 threshold) {
    u32 reg;

    reg = (func << 8) | threshold;
    __peReg[3] = reg;
}

void GXPokeAlphaRead(u32 mode) {
    u32 reg;

    reg = 0;
    SET_REG_FIELD(reg, 2, 0, mode);
    SET_REG_FIELD(reg, 1, 2, 1);
    __peReg[4] = reg;
}

void GXPokeAlphaUpdate(u8 update_enable) {
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(reg, 1, 4, update_enable);
    __peReg[1] = reg;
}

void GXPokeBlendMode(u32 type, u32 src_factor, u32 dst_factor, u32 op) {
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(reg, 1, 0, (type == GX_BM_BLEND) || (type == GX_BM_SUBTRACT));
    SET_REG_FIELD(reg, 1, 11, (type == GX_BM_SUBTRACT));
    SET_REG_FIELD(reg, 1, 1, (type == GX_BM_LOGIC));
    SET_REG_FIELD(reg, 4, 12, op);
    SET_REG_FIELD(reg, 3, 8, src_factor);
    SET_REG_FIELD(reg, 3, 5, dst_factor);
    SET_REG_FIELD(reg, 8, 24, 0x41);
    __peReg[1] = reg;
}

void GXPokeColorUpdate(u8 update_enable) {
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(reg, 1, 3, update_enable);
    __peReg[1] = reg;
}

void GXPokeDstAlpha(u8 enable, u8 alpha) {
    u32 reg;

    reg = 0;
    SET_REG_FIELD(reg, 8, 0, alpha);
    SET_REG_FIELD(reg, 1, 8, enable);
    __peReg[2] = reg;
}

void GXPokeDither(u8 dither) {
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(reg, 1, 2, dither);
    __peReg[1] = reg;
}

void GXPokeZMode(u8 compare_enable, u32 func, u8 update_enable) {
    u32 reg;

    reg = 0;
    SET_REG_FIELD(reg, 1, 0, compare_enable);
    SET_REG_FIELD(reg, 3, 1, func);
    SET_REG_FIELD(reg, 1, 4, update_enable);
    __peReg[0] = reg;
}

static void GXTokenInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    u16 token;
    OSContext exceptionContext;
    u32 reg;

    token = __peReg[7];

    if (TokenCB) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        TokenCB(token);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }

    reg = __peReg[5];
    SET_REG_FIELD(reg, 1, 2, 1);  // acknowledge the token interrupt
    __peReg[5] = reg;
}

static void GXFinishInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    u32 reg;

    reg = __peReg[5];
    SET_REG_FIELD(reg, 1, 3, 1);  // acknowledge the finish interrupt
    __peReg[5] = reg;

    DrawDone = 1;

    if (DrawDoneCB) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        DrawDoneCB();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }

    OSWakeupThread(&FinishQueue);
}

void __GXPEInit(void) {
    u32 reg;

    __OSSetInterruptHandler(18, GXTokenInterruptHandler);   // __OS_INTERRUPT_PI_PE_TOKEN
    __OSSetInterruptHandler(19, GXFinishInterruptHandler);  // __OS_INTERRUPT_PI_PE_FINISH

    OSInitThreadQueue(&FinishQueue);

    __OSUnmaskInterrupts(0x2000);  // OS_INTERRUPTMASK_PI_PE_TOKEN
    __OSUnmaskInterrupts(0x1000);  // OS_INTERRUPTMASK_PI_PE_FINISH

    // Clear pending token/finish interrupts and enable both.
    reg = __peReg[5];
    SET_REG_FIELD(reg, 1, 2, 1);
    SET_REG_FIELD(reg, 1, 3, 1);
    SET_REG_FIELD(reg, 1, 0, 1);
    SET_REG_FIELD(reg, 1, 1, 1);
    __peReg[5] = reg;
}
