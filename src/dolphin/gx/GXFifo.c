#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>
#include <dolphin/os_priv.h>
#include <dolphin/base/PPCArch.h>

#define GXFIFO_ADDR 0xCC008000

static __GXFifoObjInt* CPUFifo;
static __GXFifoObjInt* GPFifo;
static OSThread* __GXCurrentThread;
static GXBool CPGPLinked;
static BOOL GXOverflowSuspendInProgress;
static GXBreakPtCallback BreakPointCB;
static u32 __GXOverflowCount;

static void __GXFifoReadEnable(void);
static void __GXFifoReadDisable(void);
static void __GXFifoLink(u8 en);
static void __GXWriteFifoIntEnable(u8 hiWatermarkEn, u8 loWatermarkEn);
static void __GXWriteFifoIntReset(u8 hiWatermarkClr, u8 loWatermarkClr);

// FIFO watermark and breakpoint interrupts. When the CPU and GP share a FIFO,
// the writing thread is suspended at the high watermark and resumed at the low one.
static void GXCPInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;

    gx->cpStatus = __cpReg[0];

    if (GET_REG_FIELD(gx->cpEnable, 1, 3) && GET_REG_FIELD(gx->cpStatus, 1, 1)) {
        // Underflow (below the low watermark).
        OSResumeThread(__GXCurrentThread);
        GXOverflowSuspendInProgress = FALSE;
        __GXWriteFifoIntReset(TRUE, TRUE);
        __GXWriteFifoIntEnable(TRUE, FALSE);
    }

    if (GET_REG_FIELD(gx->cpEnable, 1, 2) && GET_REG_FIELD(gx->cpStatus, 1, 0)) {
        // Overflow (above the high watermark).
        __GXOverflowCount++;
        __GXWriteFifoIntEnable(FALSE, TRUE);
        __GXWriteFifoIntReset(TRUE, FALSE);
        GXOverflowSuspendInProgress = TRUE;
        OSSuspendThread(__GXCurrentThread);
    }

    if (GET_REG_FIELD(gx->cpEnable, 1, 5) && GET_REG_FIELD(gx->cpStatus, 1, 4)) {
        // Breakpoint.
        SET_REG_FIELD(gx->cpEnable, 1, 5, 0);
        __cpReg[1] = gx->cpEnable;
        if (BreakPointCB) {
            OSClearContext(&exceptionContext);
            OSSetCurrentContext(&exceptionContext);
            BreakPointCB();
            OSClearContext(&exceptionContext);
            OSSetCurrentContext(context);
        }
    }
}

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
    __GXFifoObjInt* realFifo = (__GXFifoObjInt*)fifo;

    realFifo->base = base;
    realFifo->top = (u8*)base + size - 4;
    realFifo->size = size;
    realFifo->count = 0;
    GXInitFifoLimits(fifo, size - 0x4000, (size / 2) & ~31);
    GXInitFifoPtrs(fifo, base, base);
}

void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {
    __GXFifoObjInt* realFifo = (__GXFifoObjInt*)fifo;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    realFifo->rdPtr = readPtr;
    realFifo->wrPtr = writePtr;
    realFifo->count = (u8*)writePtr - (u8*)readPtr;
    if (realFifo->count < 0) {
        realFifo->count += realFifo->size;
    }
    OSRestoreInterrupts(enabled);
}

void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark) {
    __GXFifoObjInt* realFifo = (__GXFifoObjInt*)fifo;

    realFifo->hiWatermark = hiWatermark;
    realFifo->loWatermark = loWatermark;
}

void GXSetCPUFifo(GXFifoObj* fifo) {
    __GXFifoObjInt* realFifo = (__GXFifoObjInt*)fifo;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    CPUFifo = realFifo;

    if (CPUFifo == GPFifo) {
        u32 reg;

        __piReg[3] = (u32)realFifo->base & 0x3FFFFFFF;
        __piReg[4] = (u32)realFifo->top & 0x3FFFFFFF;
        reg = (u32)realFifo->wrPtr & 0x3FFFFFE0;
        SET_REG_FIELD(reg, 1, 26, 0);  // wrap flag
        __piReg[5] = reg;

        CPGPLinked = TRUE;

        __GXWriteFifoIntReset(TRUE, TRUE);
        __GXWriteFifoIntEnable(TRUE, FALSE);
        __GXFifoLink(TRUE);
    } else {
        u32 reg;

        if (CPGPLinked) {
            __GXFifoLink(FALSE);
            CPGPLinked = FALSE;
        }

        __GXWriteFifoIntEnable(FALSE, FALSE);

        __piReg[3] = (u32)realFifo->base & 0x3FFFFFFF;
        __piReg[4] = (u32)realFifo->top & 0x3FFFFFFF;
        reg = (u32)realFifo->wrPtr & 0x3FFFFFE0;
        SET_REG_FIELD(reg, 1, 26, 0);
        __piReg[5] = reg;
    }

    __sync();

    OSRestoreInterrupts(enabled);
}

void GXSetGPFifo(GXFifoObj* fifo) {
    __GXFifoObjInt* realFifo = (__GXFifoObjInt*)fifo;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    __GXFifoReadDisable();
    __GXWriteFifoIntEnable(FALSE, FALSE);

    GPFifo = realFifo;

    __cpReg[16] = (u16)(u32)realFifo->base;
    __cpReg[18] = (u16)(u32)realFifo->top;
    __cpReg[24] = (u16)realFifo->count;
    __cpReg[26] = (u16)(u32)realFifo->wrPtr;
    __cpReg[28] = (u16)(u32)realFifo->rdPtr;
    __cpReg[20] = (u16)realFifo->hiWatermark;
    __cpReg[22] = (u16)realFifo->loWatermark;

    __cpReg[17] = (u16)(((u32)realFifo->base & 0x3FFFFFFF) >> 16);
    __cpReg[19] = (u16)(((u32)realFifo->top & 0x3FFFFFFF) >> 16);
    __cpReg[25] = (u16)(realFifo->count >> 16);
    __cpReg[27] = (u16)(((u32)realFifo->wrPtr & 0x3FFFFFFF) >> 16);
    __cpReg[29] = (u16)(((u32)realFifo->rdPtr & 0x3FFFFFFF) >> 16);
    __cpReg[21] = (u16)(realFifo->hiWatermark >> 16);
    __cpReg[23] = (u16)(realFifo->loWatermark >> 16);

    __sync();

    if (CPUFifo == GPFifo) {
        CPGPLinked = TRUE;
        __GXWriteFifoIntEnable(TRUE, FALSE);
        __GXFifoLink(TRUE);
    } else {
        CPGPLinked = FALSE;
        __GXWriteFifoIntEnable(FALSE, FALSE);
        __GXFifoLink(FALSE);
    }

    __GXWriteFifoIntReset(TRUE, TRUE);
    __GXFifoReadEnable();

    OSRestoreInterrupts(enabled);
}

void __GXFifoInit(void) {
    __OSSetInterruptHandler(17, GXCPInterruptHandler);  // __OS_INTERRUPT_PI_CP
    __OSUnmaskInterrupts(0x4000);                       // OS_INTERRUPTMASK_PI_CP
    __GXCurrentThread = OSGetCurrentThread();
    GXOverflowSuspendInProgress = FALSE;
    CPUFifo = NULL;
    GPFifo = NULL;
}

static void __GXFifoReadEnable(void) {
    SET_REG_FIELD(gx->cpEnable, 1, 0, 1);
    __cpReg[1] = gx->cpEnable;
}

static void __GXFifoReadDisable(void) {
    SET_REG_FIELD(gx->cpEnable, 1, 0, 0);
    __cpReg[1] = gx->cpEnable;
}

static void __GXFifoLink(u8 en) {
    SET_REG_FIELD(gx->cpEnable, 1, 4, (en != 0) ? 1 : 0);
    __cpReg[1] = gx->cpEnable;
}

static void __GXWriteFifoIntEnable(u8 hiWatermarkEn, u8 loWatermarkEn) {
    SET_REG_FIELD(gx->cpEnable, 1, 2, hiWatermarkEn);
    SET_REG_FIELD(gx->cpEnable, 1, 3, loWatermarkEn);
    __cpReg[1] = gx->cpEnable;
}

static void __GXWriteFifoIntReset(u8 hiWatermarkClr, u8 loWatermarkClr) {
    SET_REG_FIELD(gx->cpClr, 1, 0, hiWatermarkClr);
    SET_REG_FIELD(gx->cpClr, 1, 1, loWatermarkClr);
    __cpReg[2] = gx->cpClr;
}

// Points the write-gather pipe at ptr so GX commands go to memory instead of the FIFO.
volatile void* GXRedirectWriteGatherPipe(void* ptr) {
    u32 reg;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    GXFlush();

    while (PPCMfwpar() & 1) {
    }
    PPCMtwpar(GXFIFO_ADDR & 0x3FFFFFFF);

    if (CPGPLinked) {
        __GXFifoLink(FALSE);
        __GXWriteFifoIntEnable(FALSE, FALSE);
    }

    CPUFifo->wrPtr = (void*)((__piReg[5] & ~0x04000000) + 0x80000000);
    __piReg[3] = 0;
    __piReg[4] = 0x04000000;
    reg = (u32)ptr & 0x3FFFFFE0;
    SET_REG_FIELD(reg, 1, 26, 0);
    __piReg[5] = reg;

    __sync();

    OSRestoreInterrupts(enabled);

    return (volatile void*)GXFIFO_ADDR;
}

void GXRestoreWriteGatherPipe(void) {
    u32 reg;
    u32 i;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    // Pad out the 32-byte gather buffer.
    for (i = 0; i < 31; i++) {
        GX_WRITE_U8(0);
    }
    PPCSync();

    while (PPCMfwpar() & 1) {
    }
    PPCMtwpar(GXFIFO_ADDR & 0x3FFFFFFF);

    __piReg[3] = (u32)CPUFifo->base & 0x3FFFFFFF;
    __piReg[4] = (u32)CPUFifo->top & 0x3FFFFFFF;
    reg = (u32)CPUFifo->wrPtr & 0x3FFFFFE0;
    SET_REG_FIELD(reg, 1, 26, 0);
    __piReg[5] = reg;

    if (CPGPLinked) {
        __GXWriteFifoIntReset(TRUE, TRUE);
        __GXWriteFifoIntEnable(TRUE, FALSE);
        __GXFifoLink(TRUE);
    }

    __sync();

    OSRestoreInterrupts(enabled);
}
