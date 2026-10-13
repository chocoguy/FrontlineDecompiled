#include <dolphin/vi.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <dolphin/si.h>

#define __VIRegs ((volatile u16*)0xCC002000)

// Register indices (u16 units).
#define VI_VERT_TIMING 0
#define VI_DISP_CONFIG 1
#define VI_HORIZ_TIMING_0U 2
#define VI_HORIZ_TIMING_0L 3
#define VI_HORIZ_TIMING_1U 4
#define VI_HORIZ_TIMING_1L 5
#define VI_VERT_TIMING_ODD_U 6
#define VI_VERT_TIMING_ODD 7
#define VI_VERT_TIMING_EVEN_U 8
#define VI_VERT_TIMING_EVEN 9
#define VI_BBI_ODD_U 10
#define VI_BBI_ODD 11
#define VI_BBI_EVEN_U 12
#define VI_BBI_EVEN 13
#define VI_TOP_FIELD_BASE_LEFT_U 14
#define VI_TOP_FIELD_BASE_LEFT 15
#define VI_TOP_FIELD_BASE_RIGHT_U 16
#define VI_TOP_FIELD_BASE_RIGHT 17
#define VI_BTTM_FIELD_BASE_LEFT_U 18
#define VI_BTTM_FIELD_BASE_LEFT 19
#define VI_BTTM_FIELD_BASE_RIGHT_U 20
#define VI_BTTM_FIELD_BASE_RIGHT 21
#define VI_VERT_COUNT 22
#define VI_HORIZ_COUNT 23
#define VI_DISP_INT_0U 24
#define VI_DISP_INT_0 25
#define VI_DISP_INT_1U 26
#define VI_DISP_INT_1 27
#define VI_DISP_INT_2U 28
#define VI_DISP_INT_3U 30
#define VI_HSW 36
#define VI_HSR 37
#define VI_FCT_0U 39
#define VI_FCT_0 38
#define VI_WIDTH 56
#define VI_CLOCK_SEL 54
#define VI_DTV_STAT 55
#define VI_FB_WIDTH 56

#define VI_BITMASK(index) ((u64)1 << (63 - (index)))

#define VI_NTSC 0
#define VI_PAL 1
#define VI_MPAL 2
#define VI_DEBUG 3
#define VI_DEBUG_PAL 4
#define VI_EURGB60 5

#define VI_INTERLACE 0
#define VI_NON_INTERLACE 1
#define VI_PROGRESSIVE 2
#define VI_3D 3

#define VI_XFBMODE_SF 0
#define VI_XFBMODE_DF 1

#define VI_TVMODE(format, interlace) (((format) << 2) + (interlace))

typedef struct VITiming {
    /* 0x00 */ u8 equ;
    /* 0x02 */ u16 acv;
    /* 0x04 */ u16 prbOdd;
    /* 0x06 */ u16 prbEven;
    /* 0x08 */ u16 psbOdd;
    /* 0x0A */ u16 psbEven;
    /* 0x0C */ u8 bs1;
    /* 0x0D */ u8 bs2;
    /* 0x0E */ u8 bs3;
    /* 0x0F */ u8 bs4;
    /* 0x10 */ u16 be1;
    /* 0x12 */ u16 be2;
    /* 0x14 */ u16 be3;
    /* 0x16 */ u16 be4;
    /* 0x18 */ u16 nhlines;
    /* 0x1A */ u16 hlw;
    /* 0x1C */ u8 hsy;
    /* 0x1D */ u8 hcs;
    /* 0x1E */ u8 hce;
    /* 0x1F */ u8 hbe640;
    /* 0x20 */ u16 hbs640;
    /* 0x22 */ u8 hbeCCIR656;
    /* 0x24 */ u16 hbsCCIR656;
} VITiming;

typedef struct VIPositionInfo {
    /* 0x00 */ u16 DispPosX;
    /* 0x02 */ u16 DispPosY;
    /* 0x04 */ u16 DispSizeX;
    /* 0x06 */ u16 DispSizeY;
    /* 0x08 */ u16 AdjustedDispPosX;
    /* 0x0A */ u16 AdjustedDispPosY;
    /* 0x0C */ u16 AdjustedDispSizeY;
    /* 0x0E */ u16 AdjustedPanPosY;
    /* 0x10 */ u16 AdjustedPanSizeY;
    /* 0x12 */ u16 FBSizeX;
    /* 0x14 */ u16 FBSizeY;
    /* 0x16 */ u16 PanPosX;
    /* 0x18 */ u16 PanPosY;
    /* 0x1A */ u16 PanSizeX;
    /* 0x1C */ u16 PanSizeY;
    /* 0x20 */ u32 FBMode;
    /* 0x24 */ u32 nonInter;
    /* 0x28 */ u32 tv;
    /* 0x2C */ u8 wordPerLine;
    /* 0x2D */ u8 std;
    /* 0x2E */ u8 wpl;
    /* 0x30 */ u32 bufAddr;
    /* 0x34 */ u32 tfbb;
    /* 0x38 */ u32 bfbb;
    /* 0x3C */ u8 xof;
    /* 0x40 */ BOOL black;
    /* 0x44 */ BOOL threeD;
    /* 0x48 */ u32 rbufAddr;
    /* 0x4C */ u32 rtfbb;
    /* 0x50 */ u32 rbfbb;
    /* 0x54 */ VITiming* timing;
} VIPositionInfo;

static VITiming timing[8] = {
    {6, 240, 24, 25, 3, 2, 12, 13, 12, 13, 520, 519, 520, 519, 525, 429, 64, 71, 105, 162, 373, 122, 412},
    {6, 240, 24, 24, 4, 4, 12, 12, 12, 12, 520, 520, 520, 520, 526, 429, 64, 71, 105, 162, 373, 122, 412},
    {5, 287, 35, 36, 1, 0, 13, 12, 11, 10, 619, 618, 617, 620, 625, 432, 64, 75, 106, 172, 380, 133, 420},
    {5, 287, 33, 33, 2, 2, 13, 11, 13, 11, 619, 621, 619, 621, 624, 432, 64, 75, 106, 172, 380, 133, 420},
    {6, 240, 24, 25, 3, 2, 16, 15, 14, 13, 518, 517, 516, 519, 525, 429, 64, 78, 112, 162, 373, 122, 412},
    {6, 240, 24, 24, 4, 4, 16, 14, 16, 14, 518, 520, 518, 520, 526, 429, 64, 78, 112, 162, 373, 122, 412},
    {12, 480, 48, 48, 6, 6, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 162, 373, 122, 412},
    {12, 480, 44, 44, 10, 10, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 168, 379, 122, 412},
};

static u16 taps[25] = {
    496, 476, 430, 372, 297, 219, 142, 70, 12, 226, 203, 192, 196, 207, 222, 236, 252, 8, 15, 19, 19, 15, 12, 8, 1,
};

static u16 regs[59];
static u16 shdwRegs[59];
static VIPositionInfo HorVer;

static volatile u32 retraceCount;
static volatile u32 flushFlag;
static OSThreadQueue retraceQueue;
static VIRetraceCallback PreCB;
static VIRetraceCallback PostCB;
static u32 encoderType;
static s16 displayOffsetH;
static s16 displayOffsetV;
static volatile u32 changeMode;
static volatile u64 changed;
static volatile u32 shdwChangeMode;
static volatile u64 shdwChanged;
static VITiming* CurrTiming;
static u32 CurrTvMode;
static u32 FBSet;

static u32 getCurrentFieldEvenOdd(void);

static inline s32 cntlzd(u64 bit) {
    u32 hi;
    u32 lo;
    s32 value;

    hi = (u32)(bit >> 32);
    lo = (u32)(bit & 0xFFFFFFFF);
    value = __cntlzw(hi);
    if (value < 32) {
        return value;
    }
    return 32 + __cntlzw(lo);
}

// Copies the registers changed since the last flush to the hardware (not during a
// progressive/interlace switch's first field).
static inline BOOL VISetRegs(void) {
    s32 regIndex;

    if (!((shdwChangeMode == 1) && (getCurrentFieldEvenOdd() == 0))) {
        while (shdwChanged) {
            regIndex = cntlzd(shdwChanged);
            __VIRegs[regIndex] = shdwRegs[regIndex];
            shdwChanged &= ~VI_BITMASK(regIndex);
        }
        shdwChangeMode = 0;
        CurrTiming = HorVer.timing;
        CurrTvMode = HorVer.tv;
        return TRUE;
    }
    return FALSE;
}

static void __VIRetraceHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    u16 reg;
    u32 inter = 0;

    reg = __VIRegs[VI_DISP_INT_0U];
    if (reg & 0x8000) {
        __VIRegs[VI_DISP_INT_0U] = (u16)(reg & ~0x8000);
        inter |= 1;
    }

    reg = __VIRegs[VI_DISP_INT_1U];
    if (reg & 0x8000) {
        __VIRegs[VI_DISP_INT_1U] = (u16)(reg & ~0x8000);
        inter |= 2;
    }

    reg = __VIRegs[VI_DISP_INT_2U];
    if (reg & 0x8000) {
        __VIRegs[VI_DISP_INT_2U] = (u16)(reg & ~0x8000);
        inter |= 4;
    }

    reg = __VIRegs[VI_DISP_INT_3U];
    if (reg & 0x8000) {
        __VIRegs[VI_DISP_INT_3U] = (u16)(reg & ~0x8000);
        inter |= 8;
    }

    if ((inter & 4) || (inter & 8)) {
        OSSetCurrentContext(context);
        return;
    }

    retraceCount++;

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (PreCB) {
        (*PreCB)(retraceCount);
    }

    if (flushFlag) {
        if (VISetRegs()) {
            flushFlag = 0;
            SIRefreshSamplingRate();
        }
    }

    if (PostCB) {
        OSClearContext(&exceptionContext);
        (*PostCB)(retraceCount);
    }

    OSWakeupThread(&retraceQueue);

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback) {
    BOOL enabled;
    VIRetraceCallback oldcb;

    oldcb = PreCB;
    enabled = OSDisableInterrupts();
    PreCB = callback;
    OSRestoreInterrupts(enabled);
    return oldcb;
}

#pragma auto_inline off
static VITiming* getTiming(u32 mode) {
    switch (mode) {
    case 0:  // VI_TVMODE_NTSC_INT
        return &timing[0];
    case 1:  // VI_TVMODE_NTSC_DS
        return &timing[1];
    case 4:  // VI_TVMODE_PAL_INT
        return &timing[2];
    case 5:  // VI_TVMODE_PAL_DS
        return &timing[3];
    case 20:  // VI_TVMODE_EURGB60_INT
        return &timing[0];
    case 21:  // VI_TVMODE_EURGB60_DS
        return &timing[1];
    case 8:  // VI_TVMODE_MPAL_INT
        return &timing[4];
    case 9:  // VI_TVMODE_MPAL_DS
        return &timing[5];
    case 2:  // VI_TVMODE_NTSC_PROG
        return &timing[6];
    case 3:  // VI_TVMODE_NTSC_3D
        return &timing[7];
    case 16:  // VI_TVMODE_DEBUG_PAL_INT
        return &timing[2];
    case 17:  // VI_TVMODE_DEBUG_PAL_DS
        return &timing[3];
    }
    return NULL;
}
#pragma auto_inline reset

void __VIInit(u32 mode) {
    VITiming* tm;
    u32 nonInter;
    u32 tv;
    volatile u32 a;

    nonInter = mode & 2;
    tv = mode >> 2;

    *(u32*)0x800000CC = tv;  // TV format for the next boot
    tm = getTiming(mode);

    __VIRegs[VI_DISP_CONFIG] = 2;  // reset
    for (a = 0; a < 1000; a += 8) {
    }
    __VIRegs[VI_DISP_CONFIG] = 0;

    __VIRegs[VI_HORIZ_TIMING_0L] = (u16)tm->hlw;
    __VIRegs[VI_HORIZ_TIMING_0U] = (u16)(tm->hce | (tm->hcs << 8));
    __VIRegs[VI_HORIZ_TIMING_1L] = (u16)(tm->hsy | (tm->hbe640 << 7));
    __VIRegs[VI_HORIZ_TIMING_1U] = (u16)((tm->hbe640 >> 9) | (tm->hbs640 << 1));

    __VIRegs[VI_VERT_TIMING] = (u16)(tm->equ);
    __VIRegs[VI_VERT_TIMING_ODD] = (u16)(tm->prbOdd + tm->acv * 2 - 2);
    __VIRegs[VI_VERT_TIMING_ODD_U] = (u16)(tm->psbOdd + 2);
    __VIRegs[VI_VERT_TIMING_EVEN] = (u16)(tm->prbEven + tm->acv * 2 - 2);
    __VIRegs[VI_VERT_TIMING_EVEN_U] = (u16)(tm->psbEven + 2);

    __VIRegs[VI_BBI_ODD] = (u16)(tm->bs1 | (tm->be1 << 5));
    __VIRegs[VI_BBI_ODD_U] = (u16)(tm->bs3 | (tm->be3 << 5));
    __VIRegs[VI_BBI_EVEN] = (u16)(tm->bs2 | (tm->be2 << 5));
    __VIRegs[VI_BBI_EVEN_U] = (u16)(tm->bs4 | (tm->be4 << 5));

    __VIRegs[VI_HSW] = (u16)(40 | (40 << 8));

    __VIRegs[VI_DISP_INT_1] = 1;
    __VIRegs[VI_DISP_INT_1U] = (u16)(1 | (1 << 12));
    __VIRegs[VI_DISP_INT_0] = (u16)(tm->hlw + 1);
    __VIRegs[VI_DISP_INT_0U] = (u16)((tm->nhlines / 2 + 1) | (1 << 12));

    if (mode != 2 && mode != 3) {  // not progressive / 3D
        __VIRegs[VI_DISP_CONFIG] = (u16)(1 | (nonInter << 2) | (tv << 8));
        __VIRegs[VI_CLOCK_SEL] = 0;
    } else {
        __VIRegs[VI_DISP_CONFIG] = (u16)(1 | (1 << 2) | (tv << 8));
        __VIRegs[VI_CLOCK_SEL] = 1;
    }
}

static inline void AdjustPosition(u16 acv) {
    s32 coeff;
    s32 frac;
    s32 x;

    x = (s16)HorVer.DispPosX + displayOffsetH;
    if (x > 720 - HorVer.DispSizeX) {
        x = 720 - HorVer.DispSizeX;
    } else if (x < 0) {
        x = 0;
    }
    HorVer.AdjustedDispPosX = (u16)x;

    coeff = (HorVer.FBMode == VI_XFBMODE_SF) ? 2 : 1;
    frac = HorVer.DispPosY & 1;

    x = (s16)HorVer.DispPosY + displayOffsetV;
    HorVer.AdjustedDispPosY = (u16)((x > frac) ? x : frac);

#define DISP_TOP ((s16)HorVer.DispPosY + displayOffsetV - frac)
#define DISP_BOTTOM ((s16)HorVer.DispPosY + (s16)HorVer.DispSizeY + displayOffsetV - ((s16)acv * 2 - frac))
#define MIN0(v) (((v) < 0) ? (v) : 0)
#define MAX0(v) (((v) > 0) ? (v) : 0)
    HorVer.AdjustedDispSizeY = (u16)(HorVer.DispSizeY + MIN0(DISP_TOP) - MAX0(DISP_BOTTOM));
    HorVer.AdjustedPanPosY = (u16)(HorVer.PanPosY - MIN0(DISP_TOP) / coeff);
    HorVer.AdjustedPanSizeY = (u16)(HorVer.PanSizeY + MIN0(DISP_TOP) / coeff - MAX0(DISP_BOTTOM) / coeff);
#undef DISP_TOP
#undef DISP_BOTTOM
#undef MIN0
#undef MAX0
}

void VIInit(void) {
    u16 dspCfg;
    u32 value;
    u32 tv;
    OSSram* sram;

    encoderType = 1;

    if (!(__VIRegs[VI_DISP_CONFIG] & 1)) {
        __VIInit(0);  // VI_TVMODE_NTSC_INT
    }

    retraceCount = 0;
    changed = 0;
    shdwChanged = 0;
    changeMode = 0;
    shdwChangeMode = 0;
    flushFlag = 0;

    // Horizontal scaler filter taps.
    __VIRegs[VI_FCT_0U] = (u16)((taps[0]) | ((taps[1] & 0x3F) << 10));
    __VIRegs[VI_FCT_0] = (u16)((taps[1] >> 6) | (taps[2] << 4));
    __VIRegs[VI_FCT_0U + 2] = (u16)((taps[3]) | ((taps[4] & 0x3F) << 10));
    __VIRegs[VI_FCT_0 + 2] = (u16)((taps[4] >> 6) | (taps[5] << 4));
    __VIRegs[VI_FCT_0U + 4] = (u16)((taps[6]) | ((taps[7] & 0x3F) << 10));
    __VIRegs[VI_FCT_0 + 4] = (u16)((taps[7] >> 6) | (taps[8] << 4));
    __VIRegs[VI_FCT_0U + 6] = (u16)((taps[9]) | (taps[10] << 8));
    __VIRegs[VI_FCT_0 + 6] = (u16)((taps[11]) | (taps[12] << 8));
    __VIRegs[VI_FCT_0U + 8] = (u16)((taps[13]) | (taps[14] << 8));
    __VIRegs[VI_FCT_0 + 8] = (u16)((taps[15]) | (taps[16] << 8));
    __VIRegs[VI_FCT_0U + 10] = (u16)((taps[17]) | (taps[18] << 8));
    __VIRegs[VI_FCT_0 + 10] = (u16)((taps[19]) | (taps[20] << 8));
    __VIRegs[VI_FCT_0U + 12] = (u16)((taps[21]) | (taps[22] << 8));
    __VIRegs[VI_FCT_0 + 12] = (u16)((taps[23]) | (taps[24] << 8));

    __VIRegs[VI_WIDTH] = 640;

    sram = __OSLockSram();
    displayOffsetH = sram->displayOffsetH;
    displayOffsetV = 0;
    __OSUnlockSram(FALSE);

    dspCfg = __VIRegs[VI_DISP_CONFIG];
    HorVer.nonInter = (dspCfg >> 2) & 1;
    HorVer.tv = (dspCfg >> 8) & 3;

    tv = (HorVer.tv == VI_DEBUG) ? VI_NTSC : HorVer.tv;
    HorVer.timing = getTiming(VI_TVMODE(tv, HorVer.nonInter));
    regs[VI_DISP_CONFIG] = dspCfg;

    CurrTiming = HorVer.timing;
    CurrTvMode = HorVer.tv;

    HorVer.DispSizeX = 640;
    HorVer.DispSizeY = (u16)(CurrTiming->acv * 2);
    HorVer.DispPosX = (u16)((720 - HorVer.DispSizeX) / 2);
    HorVer.DispPosY = 0;

    AdjustPosition(CurrTiming->acv);

    HorVer.FBSizeX = 640;
    HorVer.FBSizeY = (u16)(CurrTiming->acv * 2);
    HorVer.PanPosX = 0;
    HorVer.PanPosY = 0;
    HorVer.PanSizeX = 640;
    HorVer.PanSizeY = (u16)(CurrTiming->acv * 2);
    HorVer.FBMode = VI_XFBMODE_SF;
    HorVer.wordPerLine = 40;
    HorVer.std = 40;
    HorVer.wpl = 40;
    HorVer.xof = 0;
    HorVer.black = TRUE;
    HorVer.threeD = FALSE;

    OSInitThreadQueue(&retraceQueue);

    value = __VIRegs[VI_DISP_INT_0U];
    value = value & ~0x00008000;
    __VIRegs[VI_DISP_INT_0U] = (u16)value;

    value = __VIRegs[VI_DISP_INT_1U];
    value = value & ~0x00008000;
    __VIRegs[VI_DISP_INT_1U] = (u16)value;

    PreCB = NULL;
    PostCB = NULL;

    __OSSetInterruptHandler(24, __VIRetraceHandler);  // __OS_INTERRUPT_PI_VI
    __OSUnmaskInterrupts(0x80);                      // OS_INTERRUPTMASK_PI_VI
}

void VIWaitForRetrace(void) {
    BOOL enabled;
    u32 count;

    enabled = OSDisableInterrupts();
    count = retraceCount;
    do {
        OSSleepThread(&retraceQueue);
    } while (count == retraceCount);
    OSRestoreInterrupts(enabled);
}

static inline void calcFbbs(u32 bufAddr, u16 panPosX, u16 panPosY, u8 wordPerLine, u32 xfbMode, u16 dispPosY,
                            u32* tfbb, u32* bfbb) {
    u32 bytesPerLine;
    u32 xoffInWords;

    xoffInWords = (u32)panPosX / 16;
    bytesPerLine = (u32)wordPerLine * 32;

    *tfbb = bufAddr + xoffInWords * 32 + bytesPerLine * panPosY;
    *bfbb = (xfbMode == VI_XFBMODE_SF) ? *tfbb : (*tfbb + bytesPerLine);

    if (dispPosY % 2 == 1) {
        u32 tmp = *tfbb;
        *tfbb = *bfbb;
        *bfbb = tmp;
    }

    *tfbb = *tfbb & 0x3FFFFFFF;
    *bfbb = *bfbb & 0x3FFFFFFF;
}

#define IS_LOWER_16MB(x) ((x) < 16 * 1024 * 1024)

static void setFbbRegs(VIPositionInfo* hv, u32* tfbb, u32* bfbb, u32* rtfbb, u32* rbfbb) {
    u32 shifted;

    calcFbbs(hv->bufAddr, hv->PanPosX, hv->AdjustedPanPosY, hv->wordPerLine, hv->FBMode, hv->AdjustedDispPosY, tfbb,
             bfbb);

    if (hv->threeD) {
        calcFbbs(hv->rbufAddr, hv->PanPosX, hv->AdjustedPanPosY, hv->wordPerLine, hv->FBMode, hv->AdjustedDispPosY,
                 rtfbb, rbfbb);
    }

    if (IS_LOWER_16MB(*tfbb) && IS_LOWER_16MB(*bfbb) && IS_LOWER_16MB(*rtfbb) && IS_LOWER_16MB(*rbfbb)) {
        shifted = 0;
    } else {
        shifted = 1;
    }

    if (shifted) {
        *tfbb >>= 5;
        *bfbb >>= 5;
        *rtfbb >>= 5;
        *rbfbb >>= 5;
    }

    regs[VI_TOP_FIELD_BASE_LEFT] = (u16)(*tfbb & 0xFFFF);
    changed |= VI_BITMASK(VI_TOP_FIELD_BASE_LEFT);

    regs[VI_TOP_FIELD_BASE_LEFT_U] = (u16)((*tfbb >> 16) | (hv->xof << 8) | (shifted << 12));
    changed |= VI_BITMASK(VI_TOP_FIELD_BASE_LEFT_U);

    regs[VI_BTTM_FIELD_BASE_LEFT] = (u16)(*bfbb & 0xFFFF);
    changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_LEFT);

    regs[VI_BTTM_FIELD_BASE_LEFT_U] = (u16)(*bfbb >> 16);
    changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_LEFT_U);

    if (hv->threeD) {
        regs[VI_TOP_FIELD_BASE_RIGHT] = (u16)(*rtfbb & 0xFFFF);
        changed |= VI_BITMASK(VI_TOP_FIELD_BASE_RIGHT);

        regs[VI_TOP_FIELD_BASE_RIGHT_U] = (u16)(*rtfbb >> 16);
        changed |= VI_BITMASK(VI_TOP_FIELD_BASE_RIGHT_U);

        regs[VI_BTTM_FIELD_BASE_RIGHT] = (u16)(*rbfbb & 0xFFFF);
        changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_RIGHT);

        regs[VI_BTTM_FIELD_BASE_RIGHT_U] = (u16)(*rbfbb >> 16);
        changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_RIGHT_U);
    }
}

static void setVerticalRegs(u16 dispPosY, u16 dispSizeY, u8 equ, u16 acv, u16 prbOdd, u16 prbEven, u16 psbOdd,
                            u16 psbEven, BOOL black) {
    u16 actualPrbOdd;
    u16 actualPrbEven;
    u16 actualPsbOdd;
    u16 actualPsbEven;
    u16 actualAcv;
    u16 c;
    u16 d;

    if (equ >= 10) {
        c = 1;
        d = 2;
    } else {
        c = 2;
        d = 1;
    }

    if (dispPosY % 2 == 0) {
        actualPrbOdd = (u16)(prbOdd + d * dispPosY);
        actualPsbOdd = (u16)(psbOdd + d * ((c * acv) - dispSizeY - dispPosY));
        actualPrbEven = (u16)(prbEven + d * dispPosY);
        actualPsbEven = (u16)(psbEven + d * ((c * acv) - dispSizeY - dispPosY));
    } else {
        actualPrbOdd = (u16)(prbEven + d * dispPosY);
        actualPsbOdd = (u16)(psbEven + d * ((c * acv) - dispSizeY - dispPosY));
        actualPrbEven = (u16)(prbOdd + d * dispPosY);
        actualPsbEven = (u16)(psbOdd + d * ((c * acv) - dispSizeY - dispPosY));
    }

    actualAcv = (u16)(dispSizeY / c);

    if (black) {
        actualPrbOdd += 2 * actualAcv - 2;
        actualPsbOdd += 2;
        actualPrbEven += 2 * actualAcv - 2;
        actualPsbEven += 2;
        actualAcv = 0;
    }

    regs[VI_VERT_TIMING] = (u16)(equ | (actualAcv << 4));
    changed |= VI_BITMASK(VI_VERT_TIMING);

    regs[VI_VERT_TIMING_ODD] = (u16)actualPrbOdd;
    changed |= VI_BITMASK(VI_VERT_TIMING_ODD);

    regs[VI_VERT_TIMING_ODD_U] = (u16)actualPsbOdd;
    changed |= VI_BITMASK(VI_VERT_TIMING_ODD_U);

    regs[VI_VERT_TIMING_EVEN] = (u16)actualPrbEven;
    changed |= VI_BITMASK(VI_VERT_TIMING_EVEN);

    regs[VI_VERT_TIMING_EVEN_U] = (u16)actualPsbEven;
    changed |= VI_BITMASK(VI_VERT_TIMING_EVEN_U);
}

static inline void setInterruptRegs(VITiming* tm) {
    u16 vct;
    u16 hct;
    u16 borrow;

    vct = (u16)(tm->nhlines / 2);
    borrow = (u16)(tm->nhlines % 2);
    hct = (u16)(borrow ? tm->hlw : (u16)0);

    vct++;
    hct++;

    regs[VI_DISP_INT_0] = (u16)hct;
    changed |= VI_BITMASK(VI_DISP_INT_0);

    regs[VI_DISP_INT_0U] = (u16)(vct | (1 << 12));
    changed |= VI_BITMASK(VI_DISP_INT_0U);
}

static inline void setPicConfig(u16 fbSizeX, u32 xfbMode, u16 panPosX, u16 panSizeX, u8* wordPerLine, u8* std,
                                u8* wpl, u8* xof) {
    *wordPerLine = (u8)((fbSizeX + 15) / 16);
    *std = (u8)((xfbMode == VI_XFBMODE_SF) ? *wordPerLine : (u8)(2 * *wordPerLine));
    *xof = (u8)(panPosX % 16);
    *wpl = (u8)((*xof + panSizeX + 15) / 16);

    regs[VI_HSW] = (u16)(*std | (*wpl << 8));
    changed |= VI_BITMASK(VI_HSW);
}

static inline void setBBIntervalRegs(VITiming* tm) {
    u16 val;

    val = (u16)(tm->bs1 | (tm->be1 << 5));
    regs[VI_BBI_ODD] = val;
    changed |= VI_BITMASK(VI_BBI_ODD);

    val = (u16)(tm->bs3 | (tm->be3 << 5));
    regs[VI_BBI_ODD_U] = val;
    changed |= VI_BITMASK(VI_BBI_ODD_U);

    val = (u16)(tm->bs2 | (tm->be2 << 5));
    regs[VI_BBI_EVEN] = val;
    changed |= VI_BITMASK(VI_BBI_EVEN);

    val = (u16)(tm->bs4 | (tm->be4 << 5));
    regs[VI_BBI_EVEN_U] = val;
    changed |= VI_BITMASK(VI_BBI_EVEN_U);
}

static inline void setScalingRegs(u16 panSizeX, u16 dispSizeX, BOOL threeD) {
    u32 scale;

    panSizeX = (u16)(threeD ? panSizeX * 2 : panSizeX);

    if (panSizeX < dispSizeX) {
        scale = (256 * (u32)panSizeX + (u32)dispSizeX - 1) / (u32)dispSizeX;
        regs[VI_HSR] = (u16)(scale | (1 << 12));
        changed |= VI_BITMASK(VI_HSR);

        regs[VI_FB_WIDTH] = (u16)panSizeX;
        changed |= VI_BITMASK(VI_FB_WIDTH);
    } else {
        regs[VI_HSR] = (u16)(256 | (0 << 12));
        changed |= VI_BITMASK(VI_HSR);
    }
}

static inline void setHorizontalRegs(VITiming* tm, u16 dispPosX, u16 dispSizeX) {
    u32 hbe;
    u32 hbs;
    u32 hbeLo;
    u32 hbeHi;

    regs[VI_HORIZ_TIMING_0L] = (u16)tm->hlw;
    changed |= VI_BITMASK(VI_HORIZ_TIMING_0L);

    regs[VI_HORIZ_TIMING_0U] = (u16)(tm->hce | (tm->hcs << 8));
    changed |= VI_BITMASK(VI_HORIZ_TIMING_0U);

    hbe = (u32)(tm->hbe640 - 40 + dispPosX);
    hbs = (u32)(tm->hbs640 + 40 + dispPosX - (720 - dispSizeX));

    hbeLo = hbe & 0x1FF;
    hbeHi = hbe >> 9;

    regs[VI_HORIZ_TIMING_1L] = (u16)(tm->hsy | (hbeLo << 7));
    changed |= VI_BITMASK(VI_HORIZ_TIMING_1L);

    regs[VI_HORIZ_TIMING_1U] = (u16)(hbeHi | (hbs << 1));
    changed |= VI_BITMASK(VI_HORIZ_TIMING_1U);
}

void VIConfigure(const GXRenderModeObj* rm) {
    VITiming* tm;
    u32 regDspCfg;
    BOOL enabled;
    u32 newNonInter;
    u32 tvInBootrom;
    u32 tvInGame;
    static BOOL message = FALSE;

    enabled = OSDisableInterrupts();

    newNonInter = rm->viTVmode & 3;
    if (HorVer.nonInter != newNonInter) {
        changeMode = 1;
        HorVer.nonInter = newNonInter;
    }

    tvInBootrom = *(u32*)0x800000CC;
    tvInGame = rm->viTVmode >> 2;

    if (tvInGame == VI_DEBUG_PAL && !message) {
        message = TRUE;
        OSReport("***************************************\n");
        OSReport(" ! ! ! C A U T I O N ! ! !             \n");
        OSReport("This TV format \"DEBUG_PAL\" is only for \n");
        OSReport("temporary solution until PAL DAC board \n");
        OSReport("is available. Please do NOT use this   \n");
        OSReport("mode in real games!!!                  \n");
        OSReport("***************************************\n");
    }

    if ((tvInGame == VI_NTSC) || (tvInGame == VI_MPAL)) {
        HorVer.tv = tvInBootrom;
    } else {
        HorVer.tv = tvInGame;
    }

    HorVer.DispPosX = rm->viXOrigin;
    HorVer.DispPosY = (u16)((HorVer.nonInter == VI_NON_INTERLACE) ? (u16)(rm->viYOrigin * 2) : rm->viYOrigin);
    HorVer.DispSizeX = rm->viWidth;
    HorVer.FBSizeX = rm->fbWidth;
    HorVer.FBSizeY = rm->xfbHeight;
    HorVer.FBMode = rm->xFBmode;
    HorVer.PanSizeX = HorVer.FBSizeX;
    HorVer.PanSizeY = HorVer.FBSizeY;
    HorVer.PanPosX = 0;
    HorVer.PanPosY = 0;

    HorVer.DispSizeY = (u16)((HorVer.nonInter == VI_PROGRESSIVE) ? HorVer.PanSizeY
                             : (HorVer.nonInter == VI_3D)        ? HorVer.PanSizeY
                             : (HorVer.FBMode == VI_XFBMODE_SF)  ? (u16)(2 * HorVer.PanSizeY)
                                                                 : HorVer.PanSizeY);

    HorVer.threeD = (HorVer.nonInter == VI_3D) ? TRUE : FALSE;

    tm = getTiming(VI_TVMODE(HorVer.tv, HorVer.nonInter));
    HorVer.timing = tm;

    AdjustPosition(tm->acv);
    if (encoderType == 0) {
        HorVer.tv = VI_DEBUG;
    }
    setInterruptRegs(tm);

    regDspCfg = regs[VI_DISP_CONFIG];
    if ((HorVer.nonInter == VI_PROGRESSIVE) || (HorVer.nonInter == VI_3D)) {
        regDspCfg = (regDspCfg & ~0x00000004) | (1 << 2);
    } else {
        regDspCfg = (regDspCfg & ~0x00000004) | ((HorVer.nonInter & 1) << 2);
    }

    regDspCfg = (regDspCfg & ~0x00000008) | (HorVer.threeD << 3);

    if ((HorVer.tv == VI_DEBUG_PAL) || (HorVer.tv == VI_EURGB60)) {
        regDspCfg = (regDspCfg & ~0x00000300);
    } else {
        regDspCfg = (regDspCfg & ~0x00000300) | (HorVer.tv << 8);
    }

    regs[VI_DISP_CONFIG] = (u16)regDspCfg;
    changed |= VI_BITMASK(VI_DISP_CONFIG);

    regDspCfg = regs[VI_CLOCK_SEL];
    if (rm->viTVmode == 2 || rm->viTVmode == 3) {  // VI_TVMODE_NTSC_PROG / _3D
        regDspCfg = (regDspCfg & ~1) | 1;
    } else {
        regDspCfg = (regDspCfg & ~1);
    }
    regs[VI_CLOCK_SEL] = (u16)regDspCfg;
    changed |= VI_BITMASK(VI_CLOCK_SEL);

    setScalingRegs(HorVer.PanSizeX, HorVer.DispSizeX, HorVer.threeD);
    setHorizontalRegs(tm, HorVer.AdjustedDispPosX, HorVer.DispSizeX);
    setBBIntervalRegs(tm);
    setPicConfig(HorVer.FBSizeX, HorVer.FBMode, HorVer.PanPosX, HorVer.PanSizeX, &HorVer.wordPerLine, &HorVer.std,
                 &HorVer.wpl, &HorVer.xof);

    if (FBSet) {
        setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    }

    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.AdjustedDispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven,
                    tm->psbOdd, tm->psbEven, HorVer.black);

    OSRestoreInterrupts(enabled);
}

void VIFlush(void) {
    BOOL enabled;
    s32 regIndex;

    enabled = OSDisableInterrupts();

    shdwChangeMode |= changeMode;
    changeMode = 0;

    shdwChanged |= changed;

    while (changed) {
        regIndex = cntlzd(changed);
        shdwRegs[regIndex] = regs[regIndex];
        changed &= ~VI_BITMASK(regIndex);
    }

    flushFlag = 1;
    OSRestoreInterrupts(enabled);
}

void VISetNextFrameBuffer(void* fb) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    HorVer.bufAddr = (u32)fb;
    FBSet = 1;
    setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    OSRestoreInterrupts(enabled);
}

void VISetBlack(BOOL black) {
    BOOL enabled;
    VITiming* tm;

    enabled = OSDisableInterrupts();
    HorVer.black = black;
    tm = HorVer.timing;
    // As in the original, this passes DispSizeY rather than AdjustedDispSizeY.
    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.DispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd,
                    tm->psbEven, HorVer.black);
    OSRestoreInterrupts(enabled);
}

static inline u32 getCurrentHalfLine(void) {
    u32 hcount;
    u32 vcount0;
    u32 vcount;

    vcount = __VIRegs[VI_VERT_COUNT] & 0x7FF;
    do {
        vcount0 = vcount;
        hcount = __VIRegs[VI_HORIZ_COUNT] & 0x7FF;
        vcount = __VIRegs[VI_VERT_COUNT] & 0x7FF;
    } while (vcount0 != vcount);

    return ((vcount - 1) << 1) + ((hcount - 1) / CurrTiming->hlw);
}

static u32 getCurrentFieldEvenOdd(void) {
    return (getCurrentHalfLine() < CurrTiming->nhlines) ? 1u : 0u;
}

u32 VIGetNextField(void) {
    s32 nextField;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    nextField = (s32)getCurrentFieldEvenOdd() ^ 1;
    OSRestoreInterrupts(enabled);
    return (u32)(nextField ^ (HorVer.AdjustedDispPosY & 1));
}

u32 VIGetCurrentLine(void) {
    u32 halfLine;
    VITiming* tm;
    BOOL enabled;

    tm = CurrTiming;
    enabled = OSDisableInterrupts();
    halfLine = getCurrentHalfLine();
    OSRestoreInterrupts(enabled);

    if (halfLine >= tm->nhlines) {
        halfLine -= tm->nhlines;
    }

    return halfLine >> 1;
}

u32 VIGetTvFormat(void) {
    u32 format;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    switch (CurrTvMode) {
    case VI_NTSC:
    case VI_DEBUG:
        format = VI_NTSC;
        break;
    case VI_PAL:
    case VI_DEBUG_PAL:
        format = VI_PAL;
        break;
    case VI_EURGB60:
    case VI_MPAL:
        format = CurrTvMode;
        break;
    }
    OSRestoreInterrupts(enabled);
    return format;
}
