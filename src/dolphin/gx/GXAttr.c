#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

// GXAttr
#define GX_VA_PNMTXIDX 0
#define GX_VA_TEX0MTXIDX 1
#define GX_VA_TEX1MTXIDX 2
#define GX_VA_TEX2MTXIDX 3
#define GX_VA_TEX3MTXIDX 4
#define GX_VA_TEX4MTXIDX 5
#define GX_VA_TEX5MTXIDX 6
#define GX_VA_TEX6MTXIDX 7
#define GX_VA_TEX7MTXIDX 8
#define GX_VA_NRM 10
#define GX_VA_CLR1 12
#define GX_VA_TEX1 14
#define GX_VA_TEX2 15
#define GX_VA_TEX3 16
#define GX_VA_TEX4 17
#define GX_VA_TEX5 18
#define GX_VA_TEX6 19
#define GX_VA_TEX7 20
#define GX_VA_NBT 25

#define GX_NONE 0
#define GX_TEX_ST 1
#define GX_NRM_NBT3 2
#define GX_RGB565 0

// GXTexGenType
#define GX_TG_MTX3x4 0
#define GX_TG_MTX2x4 1
#define GX_TG_BUMP0 2
#define GX_TG_BUMP7 9
#define GX_TG_SRTG 10

// GXTexGenSrc
#define GX_TG_POS 0
#define GX_TG_NRM 1
#define GX_TG_BINRM 2
#define GX_TG_TANGENT 3
#define GX_TG_TEX0 4
#define GX_TG_TEX7 11
#define GX_TG_TEXCOORD0 12
#define GX_TG_COLOR0 19
#define GX_TG_COLOR1 20

// CP register write that also shadows the array base/stride registers.
#define GX_WRITE_SOME_REG2(a, b, c, addr)        \
    do {                                         \
        long regAddr;                            \
                                                 \
        GX_WRITE_U8(a);                          \
        GX_WRITE_U8(b);                          \
        GX_WRITE_U32(c);                         \
        regAddr = addr;                          \
        if (regAddr >= 0 && regAddr < 4) {       \
            gx->indexBase[regAddr] = c;          \
        }                                        \
    } while (0)

#define GX_WRITE_SOME_REG3(a, b, c, addr)        \
    do {                                         \
        long regAddr;                            \
                                                 \
        GX_WRITE_U8(a);                          \
        GX_WRITE_U8(b);                          \
        GX_WRITE_U32(c);                         \
        regAddr = addr;                          \
        if (regAddr >= 0 && regAddr < 4) {       \
            gx->indexStride[regAddr] = c;        \
        }                                        \
    } while (0)

// Tells XF how many colours, normals and texcoords each vertex carries.
static void __GXXfVtxSpecs(void) {
    u32 nCols;
    u32 nNrm;
    u32 nTex;
    u32 reg;

    nCols = (GET_REG_FIELD(gx->vcdLo, 2, 13) ? 1 : 0) + (GET_REG_FIELD(gx->vcdLo, 2, 15) ? 1 : 0);
    nNrm = gx->hasBiNrms ? 2 : gx->hasNrms ? 1 : 0;
    nTex = 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 0) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 2) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 4) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 6) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 8) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 10) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 12) ? 1 : 0;
    nTex += GET_REG_FIELD(gx->vcdHi, 2, 14) ? 1 : 0;

    reg = nCols | (nNrm << 2) | (nTex << 4);
    GX_WRITE_XF_REG(0x08, reg);
    gx->bpSentNot = 1;
}

void GXSetVtxDesc(u32 attr, u32 type) {
    switch (attr) {
    case GX_VA_PNMTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 0, type);
        break;
    case GX_VA_TEX0MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 1, type);
        break;
    case GX_VA_TEX1MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 2, type);
        break;
    case GX_VA_TEX2MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 3, type);
        break;
    case GX_VA_TEX3MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 4, type);
        break;
    case GX_VA_TEX4MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 5, type);
        break;
    case GX_VA_TEX5MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 6, type);
        break;
    case GX_VA_TEX6MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 7, type);
        break;
    case GX_VA_TEX7MTXIDX:
        SET_REG_FIELD(gx->vcdLo, 1, 8, type);
        break;
    case GX_VA_POS:
        SET_REG_FIELD(gx->vcdLo, 2, 9, type);
        break;
    case GX_VA_NRM:
        if (type != GX_NONE) {
            gx->hasNrms = TRUE;
            gx->hasBiNrms = FALSE;
            gx->nrmType = type;
        } else {
            gx->hasNrms = FALSE;
        }
        break;
    case GX_VA_NBT:
        if (type != GX_NONE) {
            gx->hasBiNrms = TRUE;
            gx->hasNrms = FALSE;
            gx->nrmType = type;
        } else {
            gx->hasBiNrms = FALSE;
        }
        break;
    case GX_VA_CLR0:
        SET_REG_FIELD(gx->vcdLo, 2, 13, type);
        break;
    case GX_VA_CLR1:
        SET_REG_FIELD(gx->vcdLo, 2, 15, type);
        break;
    case GX_VA_TEX0:
        SET_REG_FIELD(gx->vcdHi, 2, 0, type);
        break;
    case GX_VA_TEX1:
        SET_REG_FIELD(gx->vcdHi, 2, 2, type);
        break;
    case GX_VA_TEX2:
        SET_REG_FIELD(gx->vcdHi, 2, 4, type);
        break;
    case GX_VA_TEX3:
        SET_REG_FIELD(gx->vcdHi, 2, 6, type);
        break;
    case GX_VA_TEX4:
        SET_REG_FIELD(gx->vcdHi, 2, 8, type);
        break;
    case GX_VA_TEX5:
        SET_REG_FIELD(gx->vcdHi, 2, 10, type);
        break;
    case GX_VA_TEX6:
        SET_REG_FIELD(gx->vcdHi, 2, 12, type);
        break;
    case GX_VA_TEX7:
        SET_REG_FIELD(gx->vcdHi, 2, 14, type);
        break;
    }

    if (gx->hasNrms || gx->hasBiNrms) {
        SET_REG_FIELD(gx->vcdLo, 2, 11, gx->nrmType);
    } else {
        SET_REG_FIELD(gx->vcdLo, 2, 11, 0);
    }

    gx->dirtyState |= GX_DIRTY_VCD;
}

void __GXSetVCD(void) {
    GX_WRITE_CP_STRM_REG(0x50, 0, gx->vcdLo);
    GX_WRITE_CP_STRM_REG(0x60, 0, gx->vcdHi);
    __GXXfVtxSpecs();

    // Size of a vertex in bytes when vertices are being counted (GXSetMisc).
    if (gx->vNum != 0) {
        static u8 tbl1[] = {0, 4, 1, 2};
        static u8 tbl2[] = {0, 8, 1, 2};
        static u8 tbl3[] = {0, 12, 1, 2};
        u32 vlm;
        u32 vl;
        u32 vh;

        vl = gx->vcdLo;
        vh = gx->vcdHi;
        vlm = GET_REG_FIELD(vl, 1, 0);
        vlm += GET_REG_FIELD(vl, 1, 1);
        vlm += GET_REG_FIELD(vl, 1, 2);
        vlm += GET_REG_FIELD(vl, 1, 3);
        vlm += GET_REG_FIELD(vl, 1, 4);
        vlm += GET_REG_FIELD(vl, 1, 5);
        vlm += GET_REG_FIELD(vl, 1, 6);
        vlm += GET_REG_FIELD(vl, 1, 7);
        vlm += GET_REG_FIELD(vl, 1, 8);
        vlm += tbl3[GET_REG_FIELD(vl, 2, 9)];
        vlm += tbl3[GET_REG_FIELD(vl, 2, 11)] * ((gx->hasBiNrms << 1) + 1);
        vlm += tbl1[GET_REG_FIELD(vl, 2, 13)];
        vlm += tbl1[GET_REG_FIELD(vl, 2, 15)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 0)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 2)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 4)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 6)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 8)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 10)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 12)];
        vlm += tbl2[GET_REG_FIELD(vh, 2, 14)];
        gx->vLim = vlm;
    }
}

void GXClearVtxDesc(void) {
    gx->vcdLo = 0;
    SET_REG_FIELD(gx->vcdLo, 2, 9, 1);  // GX_VA_POS = GX_DIRECT
    gx->vcdHi = 0;
    gx->hasNrms = FALSE;
    gx->hasBiNrms = FALSE;
    gx->dirtyState |= GX_DIRTY_VCD;
}

void GXSetVtxAttrFmt(u32 vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac) {
    u32* va;
    u32* vb;
    u32* vc;

    va = &gx->vatA[vtxfmt];
    vb = &gx->vatB[vtxfmt];
    vc = &gx->vatC[vtxfmt];

    switch (attr) {
    case GX_VA_POS:
        SET_REG_FIELD(*va, 1, 0, cnt);
        SET_REG_FIELD(*va, 3, 1, type);
        SET_REG_FIELD(*va, 5, 4, frac);
        break;
    case GX_VA_NRM:
    case GX_VA_NBT:
        SET_REG_FIELD(*va, 3, 10, type);
        if (cnt == GX_NRM_NBT3) {
            SET_REG_FIELD(*va, 1, 9, 1);
            SET_REG_FIELD(*va, 1, 31, 1);
        } else {
            SET_REG_FIELD(*va, 1, 9, cnt);
            SET_REG_FIELD(*va, 1, 31, 0);
        }
        break;
    case GX_VA_CLR0:
        SET_REG_FIELD(*va, 1, 13, cnt);
        SET_REG_FIELD(*va, 3, 14, type);
        break;
    case GX_VA_CLR1:
        SET_REG_FIELD(*va, 1, 17, cnt);
        SET_REG_FIELD(*va, 3, 18, type);
        break;
    case GX_VA_TEX0:
        SET_REG_FIELD(*va, 1, 21, cnt);
        SET_REG_FIELD(*va, 3, 22, type);
        SET_REG_FIELD(*va, 5, 25, frac);
        break;
    case GX_VA_TEX1:
        SET_REG_FIELD(*vb, 1, 0, cnt);
        SET_REG_FIELD(*vb, 3, 1, type);
        SET_REG_FIELD(*vb, 5, 4, frac);
        break;
    case GX_VA_TEX2:
        SET_REG_FIELD(*vb, 1, 9, cnt);
        SET_REG_FIELD(*vb, 3, 10, type);
        SET_REG_FIELD(*vb, 5, 13, frac);
        break;
    case GX_VA_TEX3:
        SET_REG_FIELD(*vb, 1, 18, cnt);
        SET_REG_FIELD(*vb, 3, 19, type);
        SET_REG_FIELD(*vb, 5, 22, frac);
        break;
    case GX_VA_TEX4:
        SET_REG_FIELD(*vb, 1, 27, cnt);
        SET_REG_FIELD(*vb, 3, 28, type);
        SET_REG_FIELD(*vc, 5, 0, frac);
        break;
    case GX_VA_TEX5:
        SET_REG_FIELD(*vc, 1, 5, cnt);
        SET_REG_FIELD(*vc, 3, 6, type);
        SET_REG_FIELD(*vc, 5, 9, frac);
        break;
    case GX_VA_TEX6:
        SET_REG_FIELD(*vc, 1, 14, cnt);
        SET_REG_FIELD(*vc, 3, 15, type);
        SET_REG_FIELD(*vc, 5, 18, frac);
        break;
    case GX_VA_TEX7:
        SET_REG_FIELD(*vc, 1, 23, cnt);
        SET_REG_FIELD(*vc, 3, 24, type);
        SET_REG_FIELD(*vc, 5, 27, frac);
        break;
    }

    gx->dirtyState |= GX_DIRTY_VAT;
    gx->dirtyVAT |= (u8)(1 << (u8)vtxfmt);
}

void __GXSetVAT(void) {
    u8 i;

    for (i = 0; i < 8; i++) {
        if (gx->dirtyVAT & (1 << i)) {
            GX_WRITE_CP_STRM_REG(0x70, i, gx->vatA[i]);
            GX_WRITE_CP_STRM_REG(0x80, i, gx->vatB[i]);
            GX_WRITE_CP_STRM_REG(0x90, i, gx->vatC[i]);
        }
    }

    gx->dirtyVAT = 0;
}

void GXGetVtxAttrFmt(u32 fmt, u32 attr, u32* cnt, u32* type, u8* frac) {
    u32* va;
    u32* vb;
    u32* vc;

    va = &gx->vatA[fmt];
    vb = &gx->vatB[fmt];
    vc = &gx->vatC[fmt];

    switch (attr) {
    case GX_VA_POS:
        *cnt = GET_REG_FIELD(*va, 1, 0);
        *type = GET_REG_FIELD(*va, 3, 1);
        *frac = (u8)GET_REG_FIELD(*va, 5, 4);
        return;
    case GX_VA_NRM:
    case GX_VA_NBT:
        *cnt = GET_REG_FIELD(*va, 1, 9);
        if (*cnt == GX_TEX_ST && (u8)GET_REG_FIELD(*va, 1, 31)) {
            *cnt = GX_NRM_NBT3;
        }
        *type = GET_REG_FIELD(*va, 3, 10);
        *frac = 0;
        return;
    case GX_VA_CLR0:
        *cnt = GET_REG_FIELD(*va, 1, 13);
        *type = GET_REG_FIELD(*va, 3, 14);
        *frac = 0;
        return;
    case GX_VA_CLR1:
        *cnt = GET_REG_FIELD(*va, 1, 17);
        *type = GET_REG_FIELD(*va, 3, 18);
        *frac = 0;
        return;
    case GX_VA_TEX0:
        *cnt = GET_REG_FIELD(*va, 1, 21);
        *type = GET_REG_FIELD(*va, 3, 22);
        *frac = (u8)GET_REG_FIELD(*va, 5, 25);
        return;
    case GX_VA_TEX1:
        *cnt = GET_REG_FIELD(*vb, 1, 0);
        *type = GET_REG_FIELD(*vb, 3, 1);
        *frac = (u8)GET_REG_FIELD(*vb, 5, 4);
        return;
    case GX_VA_TEX2:
        *cnt = GET_REG_FIELD(*vb, 1, 9);
        *type = GET_REG_FIELD(*vb, 3, 10);
        *frac = (u8)GET_REG_FIELD(*vb, 5, 13);
        return;
    case GX_VA_TEX3:
        *cnt = GET_REG_FIELD(*vb, 1, 18);
        *type = GET_REG_FIELD(*vb, 3, 19);
        *frac = (u8)GET_REG_FIELD(*vb, 5, 22);
        return;
    case GX_VA_TEX4:
        *cnt = GET_REG_FIELD(*vb, 1, 27);
        *type = GET_REG_FIELD(*vb, 3, 28);
        *frac = (u8)GET_REG_FIELD(*vc, 5, 0);
        return;
    case GX_VA_TEX5:
        *cnt = GET_REG_FIELD(*vc, 1, 5);
        *type = GET_REG_FIELD(*vc, 3, 6);
        *frac = (u8)GET_REG_FIELD(*vc, 5, 9);
        return;
    case GX_VA_TEX6:
        *cnt = GET_REG_FIELD(*vc, 1, 14);
        *type = GET_REG_FIELD(*vc, 3, 15);
        *frac = (u8)GET_REG_FIELD(*vc, 5, 18);
        return;
    case GX_VA_TEX7:
        *cnt = GET_REG_FIELD(*vc, 1, 23);
        *type = GET_REG_FIELD(*vc, 3, 24);
        *frac = (u8)GET_REG_FIELD(*vc, 5, 27);
        return;
    default:
        *cnt = GX_TEX_ST;
        *type = GX_RGB565;
        *frac = 0;
        return;
    }
}

void GXSetArray(u32 attr, const void* base_ptr, u8 stride) {
    u32 cpAttr;
    u32 phyAddr;

    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }

    cpAttr = attr - GX_VA_POS;
    phyAddr = (u32)base_ptr & 0x3FFFFFFF;

    GX_WRITE_SOME_REG2(8, cpAttr | 0xA0, phyAddr, cpAttr - 12);
    GX_WRITE_SOME_REG3(8, cpAttr | 0xB0, stride, cpAttr - 12);
}

void GXInvalidateVtxCache(void) {
    GX_WRITE_U8(0x48);
}

void GXSetTexCoordGen2(u32 dst_coord, u32 func, u32 src_param, u32 mtx, u8 normalize, u32 pt_texmtx) {
    u32 reg;
    u32 row;
    u32 form;

    reg = 0;
    form = 0;
    row = 5;

    // Input row and form (0 = ab11, 1 = abc1) for the source.
    switch (src_param) {
    case GX_TG_POS:
        row = 0;
        form = 1;
        break;
    case GX_TG_NRM:
        row = 1;
        form = 1;
        break;
    case GX_TG_BINRM:
        row = 3;
        form = 1;
        break;
    case GX_TG_TANGENT:
        row = 4;
        form = 1;
        break;
    case GX_TG_COLOR0:
        row = 2;
        break;
    case GX_TG_COLOR1:
        row = 2;
        break;
    case GX_TG_TEX0:
        row = 5;
        break;
    case GX_TG_TEX0 + 1:
        row = 6;
        break;
    case GX_TG_TEX0 + 2:
        row = 7;
        break;
    case GX_TG_TEX0 + 3:
        row = 8;
        break;
    case GX_TG_TEX0 + 4:
        row = 9;
        break;
    case GX_TG_TEX0 + 5:
        row = 10;
        break;
    case GX_TG_TEX0 + 6:
        row = 11;
        break;
    case GX_TG_TEX7:
        row = 12;
        break;
    }

    switch (func) {
    case GX_TG_MTX2x4:
        SET_REG_FIELD(reg, 1, 1, 0);
        SET_REG_FIELD(reg, 1, 2, form);
        SET_REG_FIELD(reg, 3, 4, 0);
        SET_REG_FIELD(reg, 5, 7, row);
        break;
    case GX_TG_MTX3x4:
        SET_REG_FIELD(reg, 1, 1, 1);
        SET_REG_FIELD(reg, 1, 2, form);
        SET_REG_FIELD(reg, 3, 4, 0);
        SET_REG_FIELD(reg, 5, 7, row);
        break;
    case GX_TG_BUMP0:
    case GX_TG_BUMP0 + 1:
    case GX_TG_BUMP0 + 2:
    case GX_TG_BUMP0 + 3:
    case GX_TG_BUMP0 + 4:
    case GX_TG_BUMP0 + 5:
    case GX_TG_BUMP0 + 6:
    case GX_TG_BUMP7:
        SET_REG_FIELD(reg, 1, 1, 0);
        SET_REG_FIELD(reg, 1, 2, form);
        SET_REG_FIELD(reg, 3, 4, 1);
        SET_REG_FIELD(reg, 5, 7, row);
        SET_REG_FIELD(reg, 3, 12, src_param - GX_TG_TEXCOORD0);
        SET_REG_FIELD(reg, 3, 15, func - GX_TG_BUMP0);
        break;
    case GX_TG_SRTG:
        SET_REG_FIELD(reg, 1, 1, 0);
        SET_REG_FIELD(reg, 1, 2, form);
        if (src_param == GX_TG_COLOR0) {
            SET_REG_FIELD(reg, 3, 4, 2);
        } else {
            SET_REG_FIELD(reg, 3, 4, 3);
        }
        SET_REG_FIELD(reg, 5, 7, 2);
        break;
    }

    GX_WRITE_XF_REG(dst_coord + 0x40, reg);

    reg = 0;
    SET_REG_FIELD(reg, 6, 0, pt_texmtx - 64);
    SET_REG_FIELD(reg, 1, 8, normalize);
    GX_WRITE_XF_REG(dst_coord + 0x50, reg);

    switch (dst_coord) {
    case 0:
        SET_REG_FIELD(gx->matIdxA, 6, 6, mtx);
        break;
    case 1:
        SET_REG_FIELD(gx->matIdxA, 6, 12, mtx);
        break;
    case 2:
        SET_REG_FIELD(gx->matIdxA, 6, 18, mtx);
        break;
    case 3:
        SET_REG_FIELD(gx->matIdxA, 6, 24, mtx);
        break;
    case 4:
        SET_REG_FIELD(gx->matIdxB, 6, 0, mtx);
        break;
    case 5:
        SET_REG_FIELD(gx->matIdxB, 6, 6, mtx);
        break;
    case 6:
        SET_REG_FIELD(gx->matIdxB, 6, 12, mtx);
        break;
    default:
        SET_REG_FIELD(gx->matIdxB, 6, 18, mtx);
        break;
    }

    __GXSetMatrixIndex(dst_coord + 1);  // GX_VA_TEX0MTXIDX + dst_coord
}

void GXSetNumTexGens(u8 nTexGens) {
    SET_REG_FIELD(gx->genMode, 4, 0, nTexGens);
    GX_WRITE_XF_REG(0x3F, nTexGens);
    gx->dirtyState |= GX_DIRTY_GEN_MODE;
}
