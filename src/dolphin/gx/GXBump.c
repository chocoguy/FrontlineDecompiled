#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

// GXIndTexMtxID
#define GX_ITM_OFF 0
#define GX_ITM_0 1
#define GX_ITM_1 2
#define GX_ITM_2 3
#define GX_ITM_S0 5
#define GX_ITM_S1 6
#define GX_ITM_S2 7
#define GX_ITM_T0 9
#define GX_ITM_T1 10
#define GX_ITM_T2 11

#define GX_ITW_OFF 0
#define GX_ITW_0 6

#pragma auto_inline off
void GXSetTevIndirect(u32 tev_stage, u32 ind_stage, u32 format, u32 bias_sel, u32 matrix_sel, u32 wrap_s,
                      u32 wrap_t, u8 add_prev, u8 utc_lod, u32 alpha_sel) {
    u32 reg;

    reg = 0;
    SET_REG_FIELD(reg, 2, 0, ind_stage);
    SET_REG_FIELD(reg, 2, 2, format);
    SET_REG_FIELD(reg, 3, 4, bias_sel);
    SET_REG_FIELD(reg, 2, 7, alpha_sel);
    SET_REG_FIELD(reg, 4, 9, matrix_sel);
    SET_REG_FIELD(reg, 3, 13, wrap_s);
    SET_REG_FIELD(reg, 3, 16, wrap_t);
    SET_REG_FIELD(reg, 1, 19, utc_lod);
    SET_REG_FIELD(reg, 1, 20, add_prev);
    SET_REG_FIELD(reg, 8, 24, tev_stage + 16);
    GX_WRITE_RAS_REG(reg);
    gx->bpSentNot = 0;
}
#pragma auto_inline reset

// Loads a 2x3 indirect matrix as 1.10 fixed point with a power-of-two scale
// split two bits per register.
void GXSetIndTexMtx(u32 mtx_id, f32 offset[2][3], s8 scale_exp) {
    s32 mtx[6];
    u32 reg;
    u32 id;

    switch (mtx_id) {
    case GX_ITM_0:
    case GX_ITM_1:
    case GX_ITM_2:
        id = mtx_id - 1;
        break;
    case GX_ITM_S0:
    case GX_ITM_S1:
    case GX_ITM_S2:
        id = mtx_id - 5;
        break;
    case GX_ITM_T0:
    case GX_ITM_T1:
    case GX_ITM_T2:
        id = mtx_id - 9;
        break;
    default:
        id = 0;
        break;
    }

    mtx[0] = (s32)(1024.0f * offset[0][0]) & 0x7FF;
    mtx[1] = (s32)(1024.0f * offset[1][0]) & 0x7FF;
    scale_exp += 0x11;
    reg = 0;
    SET_REG_FIELD(reg, 11, 0, mtx[0]);
    SET_REG_FIELD(reg, 11, 11, mtx[1]);
    SET_REG_FIELD(reg, 2, 22, scale_exp & 3);
    SET_REG_FIELD(reg, 8, 24, id * 3 + 6);
    GX_WRITE_RAS_REG(reg);

    mtx[2] = (s32)(1024.0f * offset[0][1]) & 0x7FF;
    mtx[3] = (s32)(1024.0f * offset[1][1]) & 0x7FF;
    reg = 0;
    SET_REG_FIELD(reg, 11, 0, mtx[2]);
    SET_REG_FIELD(reg, 11, 11, mtx[3]);
    SET_REG_FIELD(reg, 2, 22, (scale_exp >> 2) & 3);
    SET_REG_FIELD(reg, 8, 24, id * 3 + 7);
    GX_WRITE_RAS_REG(reg);

    mtx[4] = (s32)(1024.0f * offset[0][2]) & 0x7FF;
    mtx[5] = (s32)(1024.0f * offset[1][2]) & 0x7FF;
    reg = 0;
    SET_REG_FIELD(reg, 11, 0, mtx[4]);
    SET_REG_FIELD(reg, 11, 11, mtx[5]);
    SET_REG_FIELD(reg, 2, 22, (scale_exp >> 4) & 3);
    SET_REG_FIELD(reg, 8, 24, id * 3 + 8);
    GX_WRITE_RAS_REG(reg);

    gx->bpSentNot = 0;
}

void GXSetIndTexCoordScale(u32 ind_state, u32 scale_s, u32 scale_t) {
    switch (ind_state) {
    case 0:
        SET_REG_FIELD(gx->IndTexScale0, 4, 0, scale_s);
        SET_REG_FIELD(gx->IndTexScale0, 4, 4, scale_t);
        SET_REG_FIELD(gx->IndTexScale0, 8, 24, 0x25);
        GX_WRITE_RAS_REG(gx->IndTexScale0);
        break;
    case 1:
        SET_REG_FIELD(gx->IndTexScale0, 4, 8, scale_s);
        SET_REG_FIELD(gx->IndTexScale0, 4, 12, scale_t);
        SET_REG_FIELD(gx->IndTexScale0, 8, 24, 0x25);
        GX_WRITE_RAS_REG(gx->IndTexScale0);
        break;
    case 2:
        SET_REG_FIELD(gx->IndTexScale1, 4, 0, scale_s);
        SET_REG_FIELD(gx->IndTexScale1, 4, 4, scale_t);
        SET_REG_FIELD(gx->IndTexScale1, 8, 24, 0x26);
        GX_WRITE_RAS_REG(gx->IndTexScale1);
        break;
    case 3:
        SET_REG_FIELD(gx->IndTexScale1, 4, 8, scale_s);
        SET_REG_FIELD(gx->IndTexScale1, 4, 12, scale_t);
        SET_REG_FIELD(gx->IndTexScale1, 8, 24, 0x26);
        GX_WRITE_RAS_REG(gx->IndTexScale1);
        break;
    }

    gx->bpSentNot = 0;
}

void GXSetIndTexOrder(u32 ind_stage, u32 tex_coord, u32 tex_map) {
    switch (ind_stage) {
    case 0:
        SET_REG_FIELD(gx->iref, 3, 0, tex_map);
        SET_REG_FIELD(gx->iref, 3, 3, tex_coord);
        break;
    case 1:
        SET_REG_FIELD(gx->iref, 3, 6, tex_map);
        SET_REG_FIELD(gx->iref, 3, 9, tex_coord);
        break;
    case 2:
        SET_REG_FIELD(gx->iref, 3, 12, tex_map);
        SET_REG_FIELD(gx->iref, 3, 15, tex_coord);
        break;
    case 3:
        SET_REG_FIELD(gx->iref, 3, 18, tex_map);
        SET_REG_FIELD(gx->iref, 3, 21, tex_coord);
        break;
    }

    GX_WRITE_RAS_REG(gx->iref);
    gx->dirtyState |= GX_DIRTY_SU_TEX | GX_DIRTY_BP_MASK;
    gx->bpSentNot = 0;
}

void GXSetNumIndStages(u8 nIndStages) {
    SET_REG_FIELD(gx->genMode, 3, 16, nIndStages);
    gx->dirtyState |= GX_DIRTY_BP_MASK | GX_DIRTY_GEN_MODE;
}

void GXSetTevDirect(u32 tev_stage) {
    GXSetTevIndirect(tev_stage, 0, 0, 0, GX_ITM_OFF, GX_ITW_OFF, GX_ITW_OFF, FALSE, FALSE, 0);
}

void GXSetTevIndRepeat(u32 tev_stage) {
    GXSetTevIndirect(tev_stage, 0, 0, 0, GX_ITM_OFF, GX_ITW_0, GX_ITW_0, TRUE, FALSE, 0);
}

// Tells the texture unit which maps the indirect stages use.
void __GXUpdateBPMask(void) {
    u32 nIndStages;
    u32 i;
    u32 tmap;
    u32 new_imask;

    new_imask = 0;
    nIndStages = GET_REG_FIELD(gx->genMode, 3, 16);
    for (i = 0; i < nIndStages; i++) {
        switch (i) {
        case 0:
            tmap = GET_REG_FIELD(gx->iref, 3, 0);
            break;
        case 1:
            tmap = GET_REG_FIELD(gx->iref, 3, 6);
            break;
        case 2:
            tmap = GET_REG_FIELD(gx->iref, 3, 12);
            break;
        case 3:
            tmap = GET_REG_FIELD(gx->iref, 3, 18);
            break;
        }
        new_imask |= 1 << tmap;
    }

    if ((u8)gx->bpMask != new_imask) {
        SET_REG_FIELD(gx->bpMask, 8, 0, new_imask);
        GX_WRITE_RAS_REG(gx->bpMask);
        gx->bpSentNot = 0;
    }
}

void __GXFlushTextureState(void) {
    GX_WRITE_RAS_REG(gx->bpMask);
    gx->bpSentNot = 0;
}
