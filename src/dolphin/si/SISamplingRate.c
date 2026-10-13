#include <dolphin/si.h>
#include <dolphin/vi.h>

#define LATENCY 8

// Poll line interval and polls per field for each sampling rate (ms).
typedef struct XY {
    u16 line;
    u8 count;
} XY;

static XY XYNTSC[12] = {
    {246, 2}, {15, 18}, {30, 9}, {44, 6}, {52, 5}, {65, 4}, {87, 3}, {87, 3}, {87, 3}, {131, 2}, {131, 2}, {131, 2},
};

static XY XYPAL[12] = {
    {296, 2}, {15, 21}, {29, 11}, {45, 7}, {52, 6}, {63, 5}, {78, 4}, {104, 3}, {104, 3}, {104, 3}, {104, 3}, {156, 2},
};

static u32 SamplingRate;

void SISetSamplingRate(u32 msec) {
    XY* xy;
    BOOL progressive;
    BOOL enabled;

    if (11 < msec) {
        msec = 11;
    }

    enabled = OSDisableInterrupts();

    SamplingRate = msec;

    switch (VIGetTvFormat()) {
    case 0:  // VI_NTSC
    case 2:  // VI_MPAL
    case 5:  // VI_EURGB60
        xy = XYNTSC;
        break;
    case 1:  // VI_PAL
        xy = XYPAL;
        break;
    default:
        OSReport("SISetSamplingRate: unknown TV format. Use default.");
        msec = 0;
        xy = XYNTSC;
        break;
    }

    progressive = *(volatile u16*)0xCC00206C & 1;
    SISetXY((progressive ? 2u : 1u) * xy[msec].line, xy[msec].count);
    OSRestoreInterrupts(enabled);
}

void SIRefreshSamplingRate(void) {
    SISetSamplingRate(SamplingRate);
}
