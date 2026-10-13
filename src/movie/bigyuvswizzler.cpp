// tBigYUVSwizzler: one pass YUV to RGB. Each plane's samples are looked up
// in an IA8 table of their (halved) contribution to R, G and B, and the TEV
// sums them with the luma; the tBigSwizzler maps address the big planes.
#include "movie/bigyuvswizzler.h"
#include "movie/rcmp.h"
#include "dolphin/os.h"
#include "ea/real.h"

void tBigYUVSwizzler_DrawSetup(tBigYUVSwizzler* sw, GXTexObj* yTex, GXTexObj* vTex, GXTexObj* uTex) {
    tBigSwizzler* ySw = sw->ySwizzler;
    GXColor k0 = {0xFF, 0xFF, 0x00, 0x00};
    tBigSwizzler* uvSw = sw->uvSwizzler;
    GXColor k1 = {0x00, 0xFF, 0xFF, 0x00};
    f32 mtx[2][3];

    GXSetIndTexCoordScale(0, 0, 0);
    GXSetIndTexCoordScale(1, 0, 0);
    GXSetIndTexCoordScale(2, 0, 0);
    GXSetIndTexCoordScale(3, 0, 0);

    GXInitTexObjLOD(uTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXInitTexObjLOD(vTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXInitTexObjLOD(yTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTlut(&sw->tlutObj[2], 0);
    GXLoadTlut(&sw->tlutObj[1], 1);
    GXLoadTlut(&sw->tlutObj[0], 2);
    GXLoadTexObj(uTex, 0);
    GXLoadTexObj(vTex, 1);
    GXLoadTexObj(yTex, 2);
    GXLoadTexObj(&uvSw->tileTex, 3);
    GXLoadTexObj(&uvSw->pixTex, 4);
    GXLoadTexObj(&ySw->tileTex, 5);
    GXLoadTexObj(&ySw->pixTex, 6);

    GXSetTevKColor(0, k0);
    GXSetTevKColor(1, k1);
    GXSetTevSwapModeTable(1, 3, 0, 0, 0);
    GXSetTevSwapModeTable(2, 1, 3, 1, 1);

    GXSetNumTexGens(2);
    GXSetTexCoordGen2(0, 1, 0xC, 0x3C, 0, 0x7D);
    GXSetTexCoordGen2(1, 1, 0xC, 0x3C, 0, 0x7D);

    // indirect matrix 1: block coordinates to texels
    mtx[0][1] = 0.0f;
    mtx[0][0] = uvSw->blockWidth * 0.0625f;
    mtx[1][0] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][2] = 0.0f;
    mtx[1][1] = uvSw->blockHeight * 0.0625f;
    GXSetIndTexMtx(1, mtx, 4);
    // indirect matrix 2: offsets within the block
    mtx[0][1] = 0.0f;
    mtx[0][0] = 0.0625f;
    mtx[1][1] = 0.0625f;
    mtx[1][0] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][2] = 0.0f;
    GXSetIndTexMtx(2, mtx, 4);

    GXSetNumIndStages(4);
    GXSetIndTexOrder(0, 0, 3);
    GXSetTevIndirect(0, 0, 0, 0, 1, 6, 6, 0, 0, 0);
    GXSetIndTexOrder(1, 0, 4);
    GXSetTevIndirect(1, 1, 0, 2, 2, 6, 0, 1, 0, 0);
    GXSetIndTexOrder(2, 1, 5);
    GXSetTevIndirect(3, 2, 0, 0, 1, 6, 6, 0, 0, 0);
    GXSetIndTexOrder(3, 1, 6);
    GXSetTevIndirect(4, 3, 0, 2, 2, 6, 0, 1, 0, 0);

    GXSetNumTevStages(5);
    GXSetTevOrder(0, 0, 0, 0xFF);
    GXSetTevOrder(1, 0, 0, 0xFF);
    GXSetTevIndRepeat(2);
    GXSetTevOrder(2, 0, 1, 0xFF);
    GXSetTevOrder(3, 1, 2, 0xFF);
    GXSetTevOrder(4, 1, 2, 0xFF);

    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 0xF);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 7);

    GXSetTevColorOp(1, 0, 0, 0, 1, 0);
    GXSetTevColorIn(1, 0xF, 8, 0xE, 0xF);
    GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(1, 7, 7, 7, 7);
    GXSetTevSwapMode(1, 1, 2);
    GXSetTevKColorSel(1, 0xD);

    GXSetTevColorOp(2, 0, 2, 1, 0, 0);
    GXSetTevColorIn(2, 0xF, 8, 0xE, 0);
    GXSetTevAlphaOp(2, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(2, 7, 7, 7, 7);
    GXSetTevSwapMode(2, 1, 1);
    GXSetTevKColorSel(2, 0xC);

    GXSetTevColorOp(3, 0, 0, 0, 0, 0);
    GXSetTevColorIn(3, 0xF, 0xF, 0xF, 0);
    GXSetTevAlphaOp(3, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(3, 7, 7, 7, 7);

    GXSetTevColorOp(4, 0, 0, 0, 1, 0);
    GXSetTevColorIn(4, 8, 0xF, 0xF, 0);
    GXSetTevAlphaOp(4, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(4, 7, 7, 7, 4);
}

tBigYUVSwizzler* NEW_tBigYUVSwizzlerTexture(GXTexObj* yTex, GXTexObj* vTex, GXTexObj* uTex) {
    tBigYUVSwizzler* sw =
        (tBigYUVSwizzler*)MEM_alloc("tBigYUVSwizzler", 0x640, RCMP::rcmp_sys.m_allocParam);
    sw->ySwizzler = NEW_tBigSwizzlerTexture(yTex);
    sw->uvSwizzler = NEW_tBigSwizzlerTexture(vTex);

    for (int i = 0; i < 256; i++) {
        f32 c = i - 128.0f;
        sw->tlut[0][i][0] = 0xFF;
        sw->tlut[0][i][1] = i;
        sw->tlut[1][i][0] = 0.701f * c + 128.0f;  // V to R (1.402 / 2)
        sw->tlut[1][i][1] = -0.35705f * c + 85.333336f;  // V to G (-0.7141 / 2)
        sw->tlut[2][i][0] = -0.17205f * c + 42.666668f;  // U to G (-0.3441 / 2)
        sw->tlut[2][i][1] = 0.886f * c + 128.0f;  // U to B (1.772 / 2)
    }
    DCFlushRange(sw->tlut[0], 0x200);
    DCFlushRange(sw->tlut[1], 0x200);
    DCFlushRange(sw->tlut[2], 0x200);
    GXInitTlutObj(&sw->tlutObj[0], sw->tlut[0], 0, 0x100);
    GXInitTlutObj(&sw->tlutObj[1], sw->tlut[1], 0, 0x100);
    GXInitTlutObj(&sw->tlutObj[2], sw->tlut[2], 0, 0x100);
    return sw;
}

void DELETE_tBigYUVSwizzler(tBigYUVSwizzler* sw) {
    DELETE_tBigSwizzler(sw->ySwizzler);
    DELETE_tBigSwizzler(sw->uvSwizzler);
    MEM_free(sw);
}
