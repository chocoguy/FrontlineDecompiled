#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

// GXPerf0: 0-10 XF, 11-26 setup unit, 27-33 rasteriser, 34 XF wait, 35 none.
// GXPerf1: 0-8 texture unit, 9-16 vertex cache, 17-20 CP, 21 FIFO, 22 none.

#define GX_WRITE_PERF_SEL(sel)                  \
    do {                                        \
        SET_REG_FIELD(gx->perfSel, 4, 4, sel);  \
        GX_WRITE_U8(0x8);                       \
        GX_WRITE_U8(0x20);                      \
        GX_WRITE_U32(gx->perfSel);              \
    } while (0)

void GXSetGPMetric(u32 perf0, u32 perf1) {
    // Turn off whichever counters were selected before.
    switch (gx->perf0) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 34:
        GX_WRITE_XF_REG(0x06, 0);
        break;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
        GX_WRITE_RAS_REG(0x23000000);
        break;
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
        GX_WRITE_RAS_REG(0x24000000);
        break;
    default:
        break;
    }

    switch (gx->perf1) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 21:
        GX_WRITE_RAS_REG(0x67000000);
        break;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
        GX_WRITE_PERF_SEL(0);
        break;
    case 17:
    case 18:
    case 19:
    case 20:
        __cpReg[3] = 0;
        break;
    default:
        break;
    }

    gx->perf0 = perf0;
    switch (gx->perf0) {
    case 0:
        GX_WRITE_XF_REG(0x06, 0x273);
        break;
    case 1:
        GX_WRITE_XF_REG(0x06, 0x14A);
        break;
    case 2:
        GX_WRITE_XF_REG(0x06, 0x16B);
        break;
    case 3:
        GX_WRITE_XF_REG(0x06, 0x84);
        break;
    case 4:
        GX_WRITE_XF_REG(0x06, 0xC6);
        break;
    case 5:
        GX_WRITE_XF_REG(0x06, 0x210);
        break;
    case 6:
        GX_WRITE_XF_REG(0x06, 0x252);
        break;
    case 7:
        GX_WRITE_XF_REG(0x06, 0x231);
        break;
    case 8:
        GX_WRITE_XF_REG(0x06, 0x1AD);
        break;
    case 9:
        GX_WRITE_XF_REG(0x06, 0x1CE);
        break;
    case 10:
        GX_WRITE_XF_REG(0x06, 0x153);
        break;
    case 11:
        GX_WRITE_RAS_REG(0x2300AE7F);
        break;
    case 12:
        GX_WRITE_RAS_REG(0x23008E7F);
        break;
    case 13:
        GX_WRITE_RAS_REG(0x23009E7F);
        break;
    case 14:
        GX_WRITE_RAS_REG(0x23001E7F);
        break;
    case 15:
        GX_WRITE_RAS_REG(0x2300AC3F);
        break;
    case 16:
        GX_WRITE_RAS_REG(0x2300AC7F);
        break;
    case 17:
        GX_WRITE_RAS_REG(0x2300ACBF);
        break;
    case 18:
        GX_WRITE_RAS_REG(0x2300ACFF);
        break;
    case 19:
        GX_WRITE_RAS_REG(0x2300AD3F);
        break;
    case 20:
        GX_WRITE_RAS_REG(0x2300AD7F);
        break;
    case 21:
        GX_WRITE_RAS_REG(0x2300ADBF);
        break;
    case 22:
        GX_WRITE_RAS_REG(0x2300ADFF);
        break;
    case 23:
        GX_WRITE_RAS_REG(0x2300AE3F);
        break;
    case 24:
        GX_WRITE_RAS_REG(0x2300A27F);
        break;
    case 25:
        GX_WRITE_RAS_REG(0x2300A67F);
        break;
    case 26:
        GX_WRITE_RAS_REG(0x2300AA7F);
        break;
    case 27:
        GX_WRITE_RAS_REG(0x2402C0C6);
        break;
    case 28:
        GX_WRITE_RAS_REG(0x2402C16B);
        break;
    case 29:
        GX_WRITE_RAS_REG(0x2402C0E7);
        break;
    case 30:
        GX_WRITE_RAS_REG(0x2402C108);
        break;
    case 31:
        GX_WRITE_RAS_REG(0x2402C129);
        break;
    case 32:
        GX_WRITE_RAS_REG(0x2402C14A);
        break;
    case 33:
        GX_WRITE_RAS_REG(0x2402C1AD);
        break;
    case 34:
        GX_WRITE_XF_REG(0x06, 0x21);
        break;
    default:
        break;
    }

    gx->perf1 = perf1;
    switch (gx->perf1) {
    case 0:
        GX_WRITE_RAS_REG(0x67000042);
        break;
    case 1:
        GX_WRITE_RAS_REG(0x67000084);
        break;
    case 2:
        GX_WRITE_RAS_REG(0x67000063);
        break;
    case 3:
        GX_WRITE_RAS_REG(0x67000129);
        break;
    case 4:
        GX_WRITE_RAS_REG(0x6700014B);
        break;
    case 5:
        GX_WRITE_RAS_REG(0x6700018D);
        break;
    case 6:
        GX_WRITE_RAS_REG(0x670001CF);
        break;
    case 7:
        GX_WRITE_RAS_REG(0x67000211);
        break;
    case 8:
        GX_WRITE_RAS_REG(0x67000252);
        break;
    case 9:
        GX_WRITE_PERF_SEL(2);
        break;
    case 10:
        GX_WRITE_PERF_SEL(3);
        break;
    case 11:
        GX_WRITE_PERF_SEL(4);
        break;
    case 12:
        GX_WRITE_PERF_SEL(5);
        break;
    case 13:
        GX_WRITE_PERF_SEL(6);
        break;
    case 14:
        GX_WRITE_PERF_SEL(7);
        break;
    case 15:
        GX_WRITE_PERF_SEL(9);
        break;
    case 16:
        GX_WRITE_PERF_SEL(8);
        break;
    case 17:
        __cpReg[3] = 2;
        break;
    case 18:
        __cpReg[3] = 3;
        break;
    case 19:
        __cpReg[3] = 4;
        break;
    case 20:
        __cpReg[3] = 5;
        break;
    case 21:
        GX_WRITE_RAS_REG(0x67000021);
        break;
    default:
        break;
    }

    gx->bpSentNot = 0;
}

void GXClearGPMetric(void) {
    __cpReg[2] = 4;
}
