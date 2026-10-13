#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

void __GXSetDirtyState(void) {
    if (gx->dirtyState & GX_DIRTY_SU_TEX) {
        __GXSetSUTexRegs();
    }
    if (gx->dirtyState & GX_DIRTY_BP_MASK) {
        __GXUpdateBPMask();
    }
    if (gx->dirtyState & GX_DIRTY_GEN_MODE) {
        __GXSetGenMode();
    }
    if (gx->dirtyState & GX_DIRTY_VCD) {
        __GXSetVCD();
    }
    if (gx->dirtyState & GX_DIRTY_VAT) {
        __GXSetVAT();
    }
    gx->dirtyState = 0;
}

void GXBegin(u32 type, u32 vtxfmt, u16 nverts) {
    if (gx->dirtyState != 0) {
        __GXSetDirtyState();
    }

    // vNumNot and bpSentNot both clear: the last command was a register
    // write, which needs a dummy primitive to flush.
    if (*(u32*)&gx->vNumNot == 0) {
        __GXSendFlushPrim();
    }

    GX_WRITE_U8(vtxfmt | type);
    GX_WRITE_U16(nverts);
}

void __GXSendFlushPrim(void) {
    u32 i;
    u32 numD = gx->vNum * gx->vLim;

    GX_WRITE_U8(0x98);  // GX_TRIANGLESTRIP, GX_VTXFMT0
    GX_WRITE_U16(gx->vNum);
    for (i = 0; i < numD; i += 4) {
        GX_WRITE_U32(0);
    }
    gx->bpSentNot = 1;
}

void GXSetLineWidth(u8 width, u32 texOffsets) {
    SET_REG_FIELD(gx->lpSize, 8, 0, width);
    SET_REG_FIELD(gx->lpSize, 3, 16, texOffsets);
    GX_WRITE_RAS_REG(gx->lpSize);
    gx->bpSentNot = 0;
}

void GXSetPointSize(u8 pointSize, u32 texOffsets) {
    SET_REG_FIELD(gx->lpSize, 8, 8, pointSize);
    SET_REG_FIELD(gx->lpSize, 3, 19, texOffsets);
    GX_WRITE_RAS_REG(gx->lpSize);
    gx->bpSentNot = 0;
}

void GXEnableTexOffsets(u32 coord, u8 line_enable, u8 point_enable) {
    SET_REG_FIELD(gx->suTs0[coord], 1, 18, line_enable);
    SET_REG_FIELD(gx->suTs0[coord], 1, 19, point_enable);
    GX_WRITE_RAS_REG(gx->suTs0[coord]);
    gx->bpSentNot = 0;
}

void GXSetCullMode(u32 mode) {
    u32 hwMode;

    // The hardware's front/back sense is the reverse of the API's.
    switch (mode) {
    case 1:  // GX_CULL_FRONT
        hwMode = 2;
        break;
    case 2:  // GX_CULL_BACK
        hwMode = 1;
        break;
    default:
        hwMode = mode;
        break;
    }

    SET_REG_FIELD(gx->genMode, 2, 14, hwMode);
    gx->dirtyState |= GX_DIRTY_GEN_MODE;
}

void GXSetCoPlanar(u8 enable) {
    u32 reg;

    SET_REG_FIELD(gx->genMode, 1, 19, enable);
    reg = 0xFE080000;  // BP mask for the gen mode write
    GX_WRITE_RAS_REG(reg);
    GX_WRITE_RAS_REG(gx->genMode);
}

void __GXSetGenMode(void) {
    GX_WRITE_RAS_REG(gx->genMode);
    gx->bpSentNot = 0;
}
