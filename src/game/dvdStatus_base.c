// Disc drive errors. CheckDVDError polls the drive (and the reset button,
// and B+X+Start held for half a second on any pad) and, for a cover-open,
// no-disc, wrong-disc or fatal error, sets up its own minimal GX/VI state
// and draws the standard message in the IPL font until the drive recovers.
#include "game/dvdStatus_base.h"
#include "dolphin/dvd.h"
#include "dolphin/gx.h"
#include "dolphin/vi.h"
#include "dolphin/os.h"
#include "dolphin/pad.h"
#include "stdio.h"
#include "string.h"

static OSFontHeader* FontData;
static u32 LastSheet;
static s16 FontSize;
static s16 FontSpace;
static void* gDefaultFifoObj;
static void* gFrameBuffer;
static u32 gHeapPtr;  // offset from 0x80700000 of the next free memory
static void* gFifoBuf;
int gGxIsInit;
static int got_reset;
static GXRenderModeObj gRMode;

static int RFPuts(s16 x, s16 y, char* string);

// Draws `string` centred at line `y`.
static void DrawMessage(int y, char* string) {
    s32 width;
    GXSetNumTevStages(1);
    GXSetTevOp(0, 3);  // GX_REPLACE
    GXSetTevOrder(0, 0, 0, 0xFF);

    char* p = string;
    int lineWidth = 0;
    int maxWidth = 0;
    while (*p != 0) {
        if (*p == '\n') {
            if (maxWidth < lineWidth) {
                maxWidth = lineWidth;
            }
            lineWidth = 0;
        }
        p = OSGetFontWidth(p, &width);
        lineWidth += FontSize * width / FontData->cellWidth;
        lineWidth += FontSpace;
    }
    if (maxWidth < lineWidth) {
        maxWidth = lineWidth;
    }
    maxWidth = (maxWidth + 15) / 16;
    p = string;
    while (*p != 0) {  // unused length
        p++;
    }
    int x = (gRMode.fbWidth - maxWidth) / 2;
    y += FontData->leading * (FontSize / 16) / FontData->cellWidth;
    GXSetBlendMode(1, 1, 1, 0);
    RFPuts(x, y, string);
}

// Draws `string` at (x, y) in the IPL font; returns its width.
static int RFPuts(s16 x, s16 y, char* string) {
    void* image;
    s32 tx, ty, width;
    GXTexObj texObj;
    f32 m[3][4];

    LastSheet = 0;
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);   // GX_VA_POS, GX_DIRECT
    GXSetVtxDesc(13, 1);  // GX_VA_TEX0
    GXSetVtxAttrFmt(0, 9, 1, 3, 4);   // xyz s16 12.4
    GXSetVtxAttrFmt(0, 13, 1, 3, 0);  // st s16
    y = y * 16;
    s16 x16 = x * 16;
    int cx = 0;
    while (*string != 0) {
        if (*string == '\n') {
            cx = 0;
            string++;
            y = y + FontData->leading * FontSize / FontData->cellWidth;
            continue;
        }
        string = OSGetFontTexture(string, &image, &tx, &ty, &width);
        if (LastSheet != (u32)image) {
            LastSheet = (u32)image;
            GXInitTexObj(&texObj, image, FontData->sheetWidth, FontData->sheetHeight, FontData->sheetFormat, 0, 0, 0);
            GXInitTexObjLOD(&texObj, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
            GXLoadTexObj(&texObj, 0);
            PSMTXScale(m, 1.0f / FontData->sheetWidth, 1.0f / FontData->sheetHeight, 1.0f);
            GXLoadTexMtxImm(m, 0x1E, 1);  // GX_TEXMTX0, GX_MTX2x4
            GXSetNumTexGens(1);
            GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
        }
        s16 x0 = x16 + cx;
        s16 x1 = x0 + FontSize;
        s16 y0 = y - FontData->ascent * FontSize / FontData->cellWidth;
        s16 y1 = y + FontData->descent * FontSize / FontData->cellWidth;
        s16 s1 = tx + FontData->cellWidth;
        s16 t1 = ty + FontData->cellHeight;
        GXBegin(0x80, 0, 4);  // GX_QUADS
        GXWGFifo.s16 = x0;
        GXWGFifo.s16 = y0;
        GXWGFifo.s16 = 0;
        GXWGFifo.s16 = tx;
        GXWGFifo.s16 = ty;
        GXWGFifo.s16 = x1;
        GXWGFifo.s16 = y0;
        GXWGFifo.s16 = 0;
        GXWGFifo.s16 = s1;
        GXWGFifo.s16 = ty;
        GXWGFifo.s16 = x1;
        GXWGFifo.s16 = y1;
        GXWGFifo.s16 = 0;
        GXWGFifo.s16 = s1;
        GXWGFifo.s16 = t1;
        GXWGFifo.s16 = x0;
        GXWGFifo.s16 = y1;
        GXWGFifo.s16 = 0;
        GXWGFifo.s16 = tx;
        GXWGFifo.s16 = t1;
        cx += FontSize * width / FontData->cellWidth;
        cx += FontSpace;
    }
    return (cx + 15) / 16;
}

static void ShowDVDError(int error);

// 2 after an error screen was shown (the screen is left black), else 0.
extern "C" int CheckDVDError(void) {
    static u8 s_comboHeld[4];
    static u32 s_comboTime[4] = {0, 0, 0, 0};
    int result = 0;
    s32 prev = -9999;

    for (;;) {
        s32 status = DVDGetDriveStatus();
        if (OSGetResetButtonState()) {
            got_reset = 1;
        }
        if (got_reset) {
            while (OSGetResetButtonState()) {
            }
            printf("Reset button released. Reset NOW.\n");
            memset((void*)0x80003100, 0, 0x4000);
            OSResetSystem(1, 1, 0);
        }
        PADStatus pads[4];
        PADRead(pads);
        for (int i = 0; i < 4; i++) {
            if (pads[i].err == 0) {
                if ((pads[i].button & 0x1600) == 0x1600) {  // B + X + Start
                    u32 now = (u32)OSTicksToMilliseconds(OSGetTime());
                    if (!s_comboHeld[i]) {
                        s_comboTime[i] = now;
                    }
                    if (now - s_comboTime[i] > 500) {
                        OSResetSystem(1, 1, 0);
                    }
                    s_comboHeld[i] = 1;
                } else {
                    s_comboHeld[i] = 0;
                }
            }
        }
        if (status == prev) {
            continue;
        }
        if (status != 0 && status != 1) {
            printf("CheckDVDError: Trapped error %d\n", status);
        }
        prev = status;
        switch (status) {
        case -1:
        case 4:
        case 5:
        case 6:
        case 11:
            ShowDVDError(status);
            result = 2;
            continue;
        case 2:
        case 3:
        case 7:
        case 8:
        case 9:
        case 10:
            printf("CheckDVDError: Ignoring %d\n", status);
            continue;
        case 1:
            if (result != 0) {
                printf("CheckDVDError: Trapped error %d. Previous error condition, \n", status);
                printf("\tso we're waiting for everything to chill out.\n");
                continue;
            }
            break;
        }
        break;
    }
    if (result != 0) {
        VISetBlack(1);
        VIFlush();
    }
    return result;
}

static void ShowDVDError(int error) {
    f32 proj[4][4];
    f32 mv[3][4];

    GxAndVIInit();
    FontData = (OSFontHeader*)(gHeapPtr + 0x80700000);
    OSInitFont(FontData);
    GXColor black = {0, 0, 0, 0xFF};
    u16 height = gRMode.efbHeight;
    FontSize = FontData->cellWidth * 16;
    u16 width = gRMode.fbWidth;
    FontSpace = -16;
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetChanMatColor(0, black);
    GXSetBlendMode(1, 1, 0, 0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 0);
    GXSetTevOp(0, 4);  // GX_PASSCLR
    GXSetCullMode(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    C_MTXOrtho(proj, 0.0f, (int)height, 0.0f, (int)width, 0.0f, 1.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    // Black background.
    GXBegin(0x80, 0, 4);
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = width;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = width;
    GXWGFifo.u16 = height;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = height;
    GXWGFifo.u16 = 0;
    switch (error) {
    case 5:
        DrawMessage(0xC4, "The Disc Cover is open.");
        DrawMessage(0xE8, "If you want to continue the game, ");
        DrawMessage(0x104, "please close the Disc Cover.");
        break;
    case 4:
        DrawMessage(0xC4, "Please insert the");
        DrawMessage(0xE8, "Medal of Honor Frontline");
        DrawMessage(0x104, "Game Disc.");
        break;
    case 6:
        DrawMessage(0xC4, "This is not the Medal of Honor Frontline Game Disc.");
        DrawMessage(0xE8, "Please insert the Medal of Honor Frontline");
        DrawMessage(0x104, "Game Disc.");
        break;
    case 11:
        DrawMessage(0xBA, "The Game Disc could not be read.");
        DrawMessage(0xE8, "Please read the");
        DrawMessage(0x104, "Nintendo GameCube\x99 Instruction Booklet");
        DrawMessage(0x120, "for more information.");
        break;
    case -1:
        DrawMessage(0xBA, "An error has occurred.");
        DrawMessage(0xE8, "Turn off the power and refer to the");
        DrawMessage(0x104, "Nintendo GameCube\x99 Instruction Booklet");
        DrawMessage(0x120, "for further instructions.");
        break;
    }
    VISetBlack(0);
    void* fb = gFrameBuffer;
    GXCopyDisp(fb, 1);
    GXDrawDone();
    VISetNextFrameBuffer(fb);
    VIFlush();
    VIWaitForRetrace();
}

// The GX/VI setup the error screen draws with (once; the game's own may not
// be up yet). As the original: an unknown TV format leaves the mode unset.
extern "C" void GxAndVIInit(void) {
    GXRenderModeObj* mode;
    if (gGxIsInit) {
        return;
    }
    VIInit();
    switch (VIGetTvFormat()) {
    case 0:
        mode = &GXNtsc480IntDf;
        break;
    case 1:
        mode = &GXPal528IntDf;
        break;
    case 2:
        mode = &GXMpal480IntDf;
        break;
    }
    GXAdjustForOverscan(mode, &gRMode, 0, 16);
    gFrameBuffer = (void*)(gHeapPtr + 0x80700000);
    gHeapPtr += ((gRMode.fbWidth + 15) & 0xFFF0) * gRMode.xfbHeight * 2;
    gFifoBuf = (void*)(gHeapPtr + 0x80700000);
    gHeapPtr += 0x40000;
    gDefaultFifoObj = GXInit(gFifoBuf, 0x40000);
    GXSetViewport(0.0f, 0.0f, gRMode.fbWidth, gRMode.efbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, gRMode.fbWidth, gRMode.efbHeight);
    GXSetDispCopySrc(0, 0, gRMode.fbWidth, gRMode.efbHeight);
    GXSetDispCopyDst(gRMode.fbWidth, gRMode.xfbHeight);
    GXSetDispCopyYScale((f32)gRMode.xfbHeight / (f32)gRMode.efbHeight);
    GXSetCopyFilter(gRMode.aa, gRMode.sample_pattern, 1, gRMode.vfilter);
    if (gRMode.aa) {
        GXSetPixelFmt(2, 0);  // GX_PF_RGB565_Z16
    } else {
        GXSetPixelFmt(0, 0);  // GX_PF_RGB8_Z24
    }
    GXCopyDisp(gFrameBuffer, 1);
    GXSetDispCopyGamma(0);
    VIConfigure(&gRMode);
    VISetNextFrameBuffer(gFrameBuffer);
    VIFlush();
    VIWaitForRetrace();
    if (gRMode.viTVmode & 1) {  // interlaced
        VIWaitForRetrace();
    }
    gGxIsInit = 1;
}
