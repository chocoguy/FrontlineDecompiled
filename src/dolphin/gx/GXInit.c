#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>
#include <dolphin/vi.h>
#include <dolphin/base/PPCArch.h>

#define GX_TF_C4 8
#define GX_TF_C8 9
#define GX_TF_C14X2 10

static GXData gxData;
GXFifoObj FifoObj;
GXData* gx = &gxData;

volatile u32* __piReg;
volatile u16* __cpReg;
volatile u16* __peReg;
volatile u16* __memReg;

extern GXRenderModeObj GXEurgb60Hz480IntDf;

// Hands out the cache regions round-robin; colour-index textures use the CI set.
static GXTexRegion* __GXDefaultTexRegionCallback(GXTexObj* t_obj, u32 id) {
    u32 fmt;

    fmt = GXGetTexObjFmt(t_obj);
    if (fmt != GX_TF_C4 && fmt != GX_TF_C8 && fmt != GX_TF_C14X2) {
        return &gx->TexRegions[gx->nextTexRgn++ & 7];
    } else {
        return &gx->TexRegionsCI[gx->nextTexRgnCI++ & 3];
    }
}

static GXTlutRegion* __GXDefaultTlutRegionCallback(u32 idx) {
    if (idx >= 20) {
        return NULL;
    }
    return &gx->TlutRegions[idx];
}

void __GXInitGX(void);

void* GXInit(void* base, u32 size) {
    u32 i;
    u32 reg;
    u32 freqBase;

    gx->inDispList = FALSE;
    gx->dlSaveContext = TRUE;
    gx->tcsManEnab = 0;
    gx->tevTcEnab = 0;
    gx->vNum = 0;

    __piReg = (volatile u32*)0xCC003000;
    __cpReg = (volatile u16*)0xCC000000;
    __peReg = (volatile u16*)0xCC001000;
    __memReg = (volatile u16*)0xCC004000;

    __GXFifoInit();
    GXInitFifoBase(&FifoObj, base, size);
    GXSetCPUFifo(&FifoObj);
    GXSetGPFifo(&FifoObj);
    __GXPEInit();

    // Enable the write-gather pipe at the GX FIFO.
    reg = PPCMfhid2();
    PPCMtwpar(0x0C008000);
    PPCMthid2(reg | 0x40000000);

    // Shadow registers start out zero with their BP register IDs in the top byte.
    gx->genMode = 0;
    SET_REG_FIELD(gx->genMode, 8, 24, 0x00);
    gx->bpMask = 255;
    SET_REG_FIELD(gx->bpMask, 8, 24, 0x0F);
    gx->lpSize = 0;
    SET_REG_FIELD(gx->lpSize, 8, 24, 0x22);

    for (i = 0; i < 16; i++) {
        gx->tevc[i] = 0;
        gx->teva[i] = 0;
        gx->tref[i / 2] = 0;
        gx->texmapId[i] = 0xFF;  // GX_TEXMAP_NULL
        SET_REG_FIELD(gx->tevc[i], 8, 24, 0xC0 + i * 2);
        SET_REG_FIELD(gx->teva[i], 8, 24, 0xC1 + i * 2);
        SET_REG_FIELD(gx->tevKsel[i / 2], 8, 24, 0xF6 + i / 2);
        SET_REG_FIELD(gx->tref[i / 2], 8, 24, 0x28 + i / 2);
    }

    gx->iref = 0;
    SET_REG_FIELD(gx->iref, 8, 24, 0x27);

    for (i = 0; i < 8; i++) {
        gx->suTs0[i] = 0;
        gx->suTs1[i] = 0;
        SET_REG_FIELD(gx->suTs0[i], 8, 24, 0x30 + i * 2);
        SET_REG_FIELD(gx->suTs1[i], 8, 24, 0x31 + i * 2);
    }

    SET_REG_FIELD(gx->suScis0, 8, 24, 0x20);
    SET_REG_FIELD(gx->suScis1, 8, 24, 0x21);
    SET_REG_FIELD(gx->cmode0, 8, 24, 0x41);
    SET_REG_FIELD(gx->cmode1, 8, 24, 0x42);
    SET_REG_FIELD(gx->zmode, 8, 24, 0x40);
    SET_REG_FIELD(gx->peCtrl, 8, 24, 0x43);
    SET_REG_FIELD(gx->cpTex, 2, 7, 0);

    gx->dirtyState = 0;
    gx->dirtyVAT = FALSE;

    // Clock dividers for the texture and pixel engine timers.
    freqBase = OS_BUS_CLOCK / 500;
    __GXFlushTextureState();
    reg = (freqBase >> 11) | 0x400 | 0x69000000;
    GX_WRITE_RAS_REG(reg);

    __GXFlushTextureState();
    reg = (freqBase / 4224) | 0x200 | 0x46000000;
    GX_WRITE_RAS_REG(reg);

    for (i = 0; i < 8; i++) {
        SET_REG_FIELD(gx->vatA[i], 1, 30, 1);  // byte dequant
        SET_REG_FIELD(gx->vatB[i], 1, 31, 1);  // vcache enhance
        GX_WRITE_U8(0x8);
        GX_WRITE_U8(i | 0x80);
        GX_WRITE_U32(gx->vatB[i]);
    }

    {
        u32 reg1 = 0;
        u32 reg2 = 0;

        SET_REG_FIELD(reg1, 1, 0, 1);
        SET_REG_FIELD(reg1, 1, 1, 1);
        SET_REG_FIELD(reg1, 1, 2, 1);
        SET_REG_FIELD(reg1, 1, 3, 1);
        SET_REG_FIELD(reg1, 1, 4, 1);
        SET_REG_FIELD(reg1, 1, 5, 1);
        GX_WRITE_XF_REG(0x00, reg1);
        SET_REG_FIELD(reg2, 1, 0, 1);
        GX_WRITE_XF_REG(0x12, reg2);
    }

    {
        u32 reg = 0;

        SET_REG_FIELD(reg, 1, 0, 1);
        SET_REG_FIELD(reg, 1, 1, 1);
        SET_REG_FIELD(reg, 1, 2, 1);
        SET_REG_FIELD(reg, 1, 3, 1);
        SET_REG_FIELD(reg, 8, 24, 0x58);
        GX_WRITE_RAS_REG(reg);
    }

    // Default TMEM layout: 8 32K cache regions, 4 CI regions, 16+4 TLUT regions.
    for (i = 0; i < 8; i++) {
        GXInitTexCacheRegion(&gx->TexRegions[i], FALSE, i * 0x8000, 0, 0x80000 + i * 0x8000, 0);
    }

    for (i = 0; i < 4; i++) {
        GXInitTexCacheRegion(&gx->TexRegionsCI[i], FALSE, (i * 2 + 8) * 0x8000, 0, (i * 2 + 9) * 0x8000, 0);
    }

    for (i = 0; i < 16; i++) {
        GXInitTlutRegion(&gx->TlutRegions[i], 0xC0000 + i * 0x2000, 16);  // GX_TLUT_256
    }

    for (i = 0; i < 4; i++) {
        GXInitTlutRegion(&gx->TlutRegions[i + 16], 0xE0000 + i * 0x8000, 64);  // GX_TLUT_1K
    }

    __cpReg[3] = 0;

    SET_REG_FIELD(gx->perfSel, 4, 4, 0);
    GX_WRITE_U8(0x8);
    GX_WRITE_U8(0x20);
    GX_WRITE_U32(gx->perfSel);

    GX_WRITE_U8(0x10);
    GX_WRITE_U32(0x1006);
    GX_WRITE_U32(0);

    GX_WRITE_RAS_REG(0x23000000);
    GX_WRITE_RAS_REG(0x24000000);
    GX_WRITE_RAS_REG(0x67000000);

    __GXSetTmemConfig(0);
    __GXInitGX();

    return &FifoObj;
}

// Puts GX into its documented default state for the current TV format.
void __GXInitGX(void) {
    GXRenderModeObj* rmode;
    f32 identity_mtx[3][4];
    GXColor clear = {64, 64, 64, 255};
    GXColor black = {0, 0, 0, 0};
    GXColor white = {255, 255, 255, 255};
    u32 i;

    switch (VIGetTvFormat()) {
    case 0:  // VI_NTSC
        rmode = &GXNtsc480IntDf;
        break;
    case 1:  // VI_PAL
        rmode = &GXPal528IntDf;
        break;
    case 5:  // VI_EURGB60
        rmode = &GXEurgb60Hz480IntDf;
        break;
    case 2:  // VI_MPAL
        rmode = &GXMpal480IntDf;
        break;
    default:
        rmode = &GXNtsc480IntDf;
        break;
    }

    GXSetCopyClear(clear, 0xFFFFFF);

    // GX_TEXCOORDn = GX_TG_MTX2x4(GX_TG_TEXn, GX_IDENTITY), no post-transform.
    GXSetTexCoordGen2(0, 1, 4, 60, FALSE, 125);
    GXSetTexCoordGen2(1, 1, 5, 60, FALSE, 125);
    GXSetTexCoordGen2(2, 1, 6, 60, FALSE, 125);
    GXSetTexCoordGen2(3, 1, 7, 60, FALSE, 125);
    GXSetTexCoordGen2(4, 1, 8, 60, FALSE, 125);
    GXSetTexCoordGen2(5, 1, 9, 60, FALSE, 125);
    GXSetTexCoordGen2(6, 1, 10, 60, FALSE, 125);
    GXSetTexCoordGen2(7, 1, 11, 60, FALSE, 125);
    GXSetNumTexGens(1);
    GXClearVtxDesc();
    GXInvalidateVtxCache();

    for (i = GX_VA_POS; i <= 24; i++) {  // through GX_LIGHT_ARRAY
        GXSetArray(i, gx, 0);
    }

    GXSetLineWidth(6, 0);
    GXSetPointSize(6, 0);
    GXEnableTexOffsets(0, FALSE, FALSE);
    GXEnableTexOffsets(1, FALSE, FALSE);
    GXEnableTexOffsets(2, FALSE, FALSE);
    GXEnableTexOffsets(3, FALSE, FALSE);
    GXEnableTexOffsets(4, FALSE, FALSE);
    GXEnableTexOffsets(5, FALSE, FALSE);
    GXEnableTexOffsets(6, FALSE, FALSE);
    GXEnableTexOffsets(7, FALSE, FALSE);

    identity_mtx[0][0] = 1.0f;
    identity_mtx[0][1] = 0.0f;
    identity_mtx[0][2] = 0.0f;
    identity_mtx[0][3] = 0.0f;
    identity_mtx[1][0] = 0.0f;
    identity_mtx[1][1] = 1.0f;
    identity_mtx[1][2] = 0.0f;
    identity_mtx[1][3] = 0.0f;
    identity_mtx[2][0] = 0.0f;
    identity_mtx[2][1] = 0.0f;
    identity_mtx[2][2] = 1.0f;
    identity_mtx[2][3] = 0.0f;
    GXLoadPosMtxImm(identity_mtx, GX_PNMTX0);
    GXLoadNrmMtxImm(identity_mtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXLoadTexMtxImm(identity_mtx, GX_IDENTITY, 0);  // GX_MTX3x4
    GXLoadTexMtxImm(identity_mtx, 125, 0);          // GX_PTIDENTITY

    GXSetViewport(0.0f, 0.0f, rmode->fbWidth, rmode->xfbHeight, 0.0f, 1.0f);
    GXSetCoPlanar(FALSE);
    GXSetCullMode(2);  // GX_CULL_BACK
    GXSetClipMode(0);  // GX_CLIP_ENABLE
    GXSetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetScissorBoxOffset(0, 0);

    GXSetNumChans(0);
    GXSetChanCtrl(4, FALSE, 0, 1, 0, 0, 2);  // GX_COLOR0A0, reg/vtx, no lights, GX_AF_NONE
    GXSetChanAmbColor(4, black);
    GXSetChanMatColor(4, white);
    GXSetChanCtrl(5, FALSE, 0, 1, 0, 0, 2);  // GX_COLOR1A1
    GXSetChanAmbColor(5, black);
    GXSetChanMatColor(5, white);

    GXInvalidateTexAll();
    gx->nextTexRgn = 0;
    gx->nextTexRgnCI = 0;
    GXSetTexRegionCallback(__GXDefaultTexRegionCallback);
    GXSetTlutRegionCallback(__GXDefaultTlutRegionCallback);

    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOrder(1, 1, 1, 4);
    GXSetTevOrder(2, 2, 2, 4);
    GXSetTevOrder(3, 3, 3, 4);
    GXSetTevOrder(4, 4, 4, 4);
    GXSetTevOrder(5, 5, 5, 4);
    GXSetTevOrder(6, 6, 6, 4);
    GXSetTevOrder(7, 7, 7, 4);
    GXSetTevOrder(8, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(9, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(10, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(11, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(12, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(13, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(14, 0xFF, 0xFF, 0xFF);
    GXSetTevOrder(15, 0xFF, 0xFF, 0xFF);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 3);                 // GX_REPLACE
    GXSetAlphaCompare(7, 0, 0, 7, 0);  // GX_ALWAYS
    GXSetZTexture(0, 0x11, 0);        // GX_ZT_DISABLE, GX_TF_Z8

    for (i = 0; i < 16; i++) {
        GXSetTevKColorSel(i, 6);  // GX_TEV_KCSEL_1_4
        GXSetTevKAlphaSel(i, 0);  // GX_TEV_KASEL_1
        GXSetTevSwapMode(i, 0, 0);
    }

    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 0, 0, 3);
    GXSetTevSwapModeTable(2, 1, 1, 1, 3);
    GXSetTevSwapModeTable(3, 2, 2, 2, 3);

    for (i = 0; i < 16; i++) {
        GXSetTevDirect(i);
    }

    GXSetNumIndStages(0);
    GXSetIndTexCoordScale(0, 0, 0);
    GXSetIndTexCoordScale(1, 0, 0);
    GXSetIndTexCoordScale(2, 0, 0);
    GXSetIndTexCoordScale(3, 0, 0);

    GXSetFog(0, 0.0f, 1.0f, 0.1f, 1.0f, black);
    GXSetFogRangeAdj(FALSE, 0, NULL);
    GXSetBlendMode(0, 4, 5, 0);  // GX_BM_NONE, SRCALPHA, INVSRCALPHA, GX_LO_CLEAR
    GXSetColorUpdate(TRUE);
    GXSetAlphaUpdate(TRUE);
    GXSetZMode(TRUE, 3, TRUE);  // GX_LEQUAL
    GXSetZCompLoc(TRUE);
    GXSetDither(TRUE);
    GXSetDstAlpha(FALSE, 0);
    GXSetPixelFmt(0, 0);  // GX_PF_RGB8_Z24, GX_ZC_LINEAR
    GXSetFieldMask(TRUE, TRUE);
    GXSetFieldMode(rmode->field_rendering, (rmode->viHeight == 2 * rmode->xfbHeight) ? TRUE : FALSE);

    GXSetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyDst(rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyYScale((f32)rmode->xfbHeight / (f32)rmode->efbHeight);
    GXSetCopyClamp(3);  // GX_CLAMP_TOP | GX_CLAMP_BOTTOM
    GXSetCopyFilter(rmode->aa, rmode->sample_pattern, TRUE, rmode->vfilter);
    GXSetDispCopyGamma(0);        // GX_GM_1_0
    GXSetDispCopyFrame2Field(0);  // GX_COPY_PROGRESSIVE
    GXClearBoundingBox();

    GXPokeColorUpdate(TRUE);
    GXPokeAlphaUpdate(TRUE);
    GXPokeDither(FALSE);
    GXPokeBlendMode(0, 0, 1, 15);  // GX_BM_NONE, GX_BL_ZERO, GX_BL_ONE, GX_LO_SET
    GXPokeAlphaMode(7, 0);         // GX_ALWAYS
    GXPokeAlphaRead(1);            // GX_READ_FF
    GXPokeDstAlpha(FALSE, 0);
    GXPokeZMode(TRUE, 7, TRUE);    // GX_ALWAYS

    GXSetGPMetric(0x23, 0x16);  // GX_PERF0_NONE, GX_PERF1_NONE
    GXClearGPMetric();
}
