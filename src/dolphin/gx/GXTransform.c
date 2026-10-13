#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

#define GX_PTTEXMTX0 64
#define GX_MTX3x4 0
#define GX_MTX2x4 1
#define GX_VA_PNMTXIDX 0
#define GX_VA_TEX4MTXIDX 5

void GXSetProjection(const f32 mtx[4][4], GXProjectionType type) {
    u32 reg;

    gx->projType = type;
    gx->projMtx[0] = mtx[0][0];
    gx->projMtx[2] = mtx[1][1];
    gx->projMtx[4] = mtx[2][2];
    gx->projMtx[5] = mtx[2][3];
    if (type == GX_ORTHOGRAPHIC) {
        gx->projMtx[1] = mtx[0][3];
        gx->projMtx[3] = mtx[1][3];
    } else {
        gx->projMtx[1] = mtx[0][2];
        gx->projMtx[3] = mtx[1][2];
    }

    reg = 0x00061020;  // 7 words at XF 0x1020
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    GX_WRITE_F32(gx->projMtx[0]);
    GX_WRITE_F32(gx->projMtx[1]);
    GX_WRITE_F32(gx->projMtx[2]);
    GX_WRITE_F32(gx->projMtx[3]);
    GX_WRITE_F32(gx->projMtx[4]);
    GX_WRITE_F32(gx->projMtx[5]);
    GX_WRITE_U32(gx->projType);
    gx->bpSentNot = 1;
}

void GXSetProjectionv(const f32* ptr) {
    u32 reg;

    gx->projType = (u32)ptr[0];
    gx->projMtx[0] = ptr[1];
    gx->projMtx[1] = ptr[2];
    gx->projMtx[2] = ptr[3];
    gx->projMtx[3] = ptr[4];
    gx->projMtx[4] = ptr[5];
    gx->projMtx[5] = ptr[6];

    reg = 0x00061020;
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    GX_WRITE_F32(gx->projMtx[0]);
    GX_WRITE_F32(gx->projMtx[1]);
    GX_WRITE_F32(gx->projMtx[2]);
    GX_WRITE_F32(gx->projMtx[3]);
    GX_WRITE_F32(gx->projMtx[4]);
    GX_WRITE_F32(gx->projMtx[5]);
    GX_WRITE_U32(gx->projType);
    gx->bpSentNot = 1;
}

void GXGetProjectionv(f32* ptr) {
    ptr[0] = gx->projType;
    ptr[1] = gx->projMtx[0];
    ptr[2] = gx->projMtx[1];
    ptr[3] = gx->projMtx[2];
    ptr[4] = gx->projMtx[3];
    ptr[5] = gx->projMtx[4];
    ptr[6] = gx->projMtx[5];
}

// Paired-single copies of a matrix into the write-gather pipe.
static asm void WriteMTXPS4x3(register const f32 mtx[3][4], register volatile f32* dest) {
    nofralloc
    psq_l f0, 0x0(r3), 0, 0
    psq_l f1, 0x8(r3), 0, 0
    psq_l f2, 0x10(r3), 0, 0
    psq_l f3, 0x18(r3), 0, 0
    psq_l f4, 0x20(r3), 0, 0
    psq_l f5, 0x28(r3), 0, 0
    psq_st f0, 0x0(r4), 0, 0
    psq_st f1, 0x0(r4), 0, 0
    psq_st f2, 0x0(r4), 0, 0
    psq_st f3, 0x0(r4), 0, 0
    psq_st f4, 0x0(r4), 0, 0
    psq_st f5, 0x0(r4), 0, 0
    blr
}

static asm void WriteMTXPS3x3from3x4(register const f32 mtx[3][4], register volatile f32* dest) {
    nofralloc
    psq_l f0, 0x0(r3), 0, 0
    lfs f1, 0x8(r3)
    psq_l f2, 0x10(r3), 0, 0
    lfs f3, 0x18(r3)
    psq_l f4, 0x20(r3), 0, 0
    lfs f5, 0x28(r3)
    psq_st f0, 0x0(r4), 0, 0
    stfs f1, 0x0(r4)
    psq_st f2, 0x0(r4), 0, 0
    stfs f3, 0x0(r4)
    psq_st f4, 0x0(r4), 0, 0
    stfs f5, 0x0(r4)
    blr
}

static asm void WriteMTXPS4x2(register const f32 mtx[2][4], register volatile f32* dest) {
    nofralloc
    psq_l f0, 0x0(r3), 0, 0
    psq_l f1, 0x8(r3), 0, 0
    psq_l f2, 0x10(r3), 0, 0
    psq_l f3, 0x18(r3), 0, 0
    psq_st f0, 0x0(r4), 0, 0
    psq_st f1, 0x0(r4), 0, 0
    psq_st f2, 0x0(r4), 0, 0
    psq_st f3, 0x0(r4), 0, 0
    blr
}

void GXLoadPosMtxImm(const f32 mtx[3][4], u32 id) {
    u32 reg;
    u32 addr;

    addr = id * 4;
    reg = addr | 0xB0000;  // 12 words
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    WriteMTXPS4x3(mtx, &__GXWGFifo.f32);
}

void GXLoadPosMtxIndx(u16 mtx_indx, u32 id) {
    u32 offset;
    u32 reg;

    offset = id * 4;
    reg = 0;
    SET_REG_FIELD(reg, 12, 0, offset);
    SET_REG_FIELD(reg, 4, 12, 11);
    SET_REG_FIELD(reg, 16, 16, mtx_indx);
    GX_WRITE_U8(0x20);
    GX_WRITE_U32(reg);
}

void GXLoadNrmMtxImm(const f32 mtx[3][4], u32 id) {
    u32 reg;
    u32 addr;

    addr = id * 3 + 0x400;
    reg = addr | 0x80000;  // 9 words
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    WriteMTXPS3x3from3x4(mtx, &__GXWGFifo.f32);
}

void GXSetCurrentMtx(u32 id) {
    SET_REG_FIELD(gx->matIdxA, 6, 0, id);
    __GXSetMatrixIndex(GX_VA_PNMTXIDX);
}

void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, u32 type) {
    u32 reg;
    u32 addr;
    u32 count;

    if (id >= GX_PTTEXMTX0) {
        addr = (id - GX_PTTEXMTX0) * 4 + 0x500;
    } else {
        addr = id * 4;
    }

    count = (type == GX_MTX2x4) ? 8 : 12;
    reg = addr | ((count - 1) << 16);
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);

    if (type == GX_MTX3x4) {
        WriteMTXPS4x3(mtx, &__GXWGFifo.f32);
    } else {
        WriteMTXPS4x2(mtx, &__GXWGFifo.f32);
    }
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {
    f32 sx;
    f32 sy;
    f32 sz;
    f32 ox;
    f32 oy;
    f32 oz;
    f32 zmin;
    f32 zmax;
    u32 reg;

    // Interlaced even field: offset by half a line.
    if (field == 0) {
        top -= 0.5f;
    }

    sx = wd / 2.0f;
    sy = -ht / 2.0f;
    ox = 342.0f + (left + (wd / 2.0f));
    oy = 342.0f + (top + (ht / 2.0f));
    zmin = 1.6777215e7f * nearz;
    zmax = 1.6777215e7f * farz;
    sz = zmax - zmin;
    oz = zmax;

    gx->vpLeft = left;
    gx->vpTop = top;
    gx->vpWd = wd;
    gx->vpHt = ht;
    gx->vpNearz = nearz;
    gx->vpFarz = farz;

    if (gx->fgRange) {
        __GXSetRange(nearz, gx->fgSideX);
    }

    reg = 0x5101A;  // 6 words at XF 0x101A
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    GX_WRITE_F32(sx);
    GX_WRITE_F32(sy);
    GX_WRITE_F32(sz);
    GX_WRITE_F32(ox);
    GX_WRITE_F32(oy);
    GX_WRITE_F32(oz);
    gx->bpSentNot = 1;
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1);
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    u32 tp;
    u32 lf;
    u32 bm;
    u32 rt;

    tp = top + 342;
    lf = left + 342;
    bm = tp + ht - 1;
    rt = lf + wd - 1;

    SET_REG_FIELD(gx->suScis0, 11, 0, tp);
    SET_REG_FIELD(gx->suScis0, 11, 12, lf);
    SET_REG_FIELD(gx->suScis1, 11, 0, bm);
    SET_REG_FIELD(gx->suScis1, 11, 12, rt);

    GX_WRITE_RAS_REG(gx->suScis0);
    GX_WRITE_RAS_REG(gx->suScis1);
    gx->bpSentNot = 0;
}

void GXSetScissorBoxOffset(s32 x_off, s32 y_off) {
    u32 reg;
    u32 hx;
    u32 hy;

    reg = 0;
    hx = (u32)(x_off + 342) >> 1;
    hy = (u32)(y_off + 342) >> 1;
    SET_REG_FIELD(reg, 10, 0, hx);
    SET_REG_FIELD(reg, 10, 10, hy);
    SET_REG_FIELD(reg, 8, 24, 0x59);
    GX_WRITE_RAS_REG(reg);
    gx->bpSentNot = 0;
}

void GXSetClipMode(u32 mode) {
    GX_WRITE_XF_REG(0x05, mode);
    gx->bpSentNot = 1;
}

void __GXSetMatrixIndex(u32 matIdxAttr) {
    if (matIdxAttr < GX_VA_TEX4MTXIDX) {
        GX_WRITE_CP_STRM_REG(0x30, 0, gx->matIdxA);
        GX_WRITE_XF_REG(0x18, gx->matIdxA);
    } else {
        GX_WRITE_CP_STRM_REG(0x40, 0, gx->matIdxB);
        GX_WRITE_XF_REG(0x19, gx->matIdxB);
    }
    gx->bpSentNot = 1;
}
