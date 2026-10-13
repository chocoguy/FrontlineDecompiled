#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

// GXTevMode
#define GX_MODULATE 0
#define GX_DECAL 1
#define GX_BLEND 2
#define GX_REPLACE 3
#define GX_PASSCLR 4

// GXTevColorArg
#define GX_CC_CPREV 0
#define GX_CC_TEXC 8
#define GX_CC_TEXA 9
#define GX_CC_RASC 10
#define GX_CC_ONE 12
#define GX_CC_ZERO 15

// GXTevAlphaArg
#define GX_CA_APREV 0
#define GX_CA_TEXA 4
#define GX_CA_RASA 5
#define GX_CA_ZERO 7

#define GX_TEV_ADD 0
#define GX_TB_ZERO 0
#define GX_CS_SCALE_1 0
#define GX_TEVPREV 0

#define GX_TF_Z8 0x11
#define GX_TF_Z16 0x13
#define GX_TF_Z24X8 0x16

#define GX_MAX_TEXMAP 8
#define GX_MAX_TEXCOORD 8
#define GX_TEXMAP_NULL 0xFF
#define GX_COLOR_NULL 0xFF

void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u8 clamp, u32 out_reg);
void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, u8 clamp, u32 out_reg);

// Presets: stage 0 takes the rasterised colour, later stages the previous stage.
void GXSetTevOp(u32 id, u32 mode) {
    u32 carg;
    u32 aarg;

    carg = GX_CC_RASC;
    aarg = GX_CA_RASA;
    if (id != 0) {
        carg = GX_CC_CPREV;
        aarg = GX_CA_APREV;
    }

    switch (mode) {
    case GX_MODULATE:
        GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_TEXC, carg, GX_CC_ZERO);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
        break;
    case GX_DECAL:
        GXSetTevColorIn(id, carg, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
        break;
    case GX_BLEND:
        GXSetTevColorIn(id, carg, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
        break;
    case GX_REPLACE:
        GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        break;
    case GX_PASSCLR:
        GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, carg);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
        break;
    }

    GXSetTevColorOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, TRUE, GX_TEVPREV);
}

void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    u32* pTevReg;

    pTevReg = &gx->tevc[stage];
    SET_REG_FIELD(*pTevReg, 4, 12, a);
    SET_REG_FIELD(*pTevReg, 4, 8, b);
    SET_REG_FIELD(*pTevReg, 4, 4, c);
    SET_REG_FIELD(*pTevReg, 4, 0, d);

    GX_WRITE_RAS_REG(*pTevReg);
    gx->bpSentNot = 0;
}

void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    u32* pTevReg;

    pTevReg = &gx->teva[stage];
    SET_REG_FIELD(*pTevReg, 3, 13, a);
    SET_REG_FIELD(*pTevReg, 3, 10, b);
    SET_REG_FIELD(*pTevReg, 3, 7, c);
    SET_REG_FIELD(*pTevReg, 3, 4, d);

    GX_WRITE_RAS_REG(*pTevReg);
    gx->bpSentNot = 0;
}

// Ops above GX_TEV_SUB are the compare ops, which reuse the scale field and set bias to 3.
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u8 clamp, u32 out_reg) {
    u32* pTevReg;

    pTevReg = &gx->tevc[stage];
    SET_REG_FIELD(*pTevReg, 1, 18, op & 1);
    if (op <= 1) {
        SET_REG_FIELD(*pTevReg, 2, 20, scale);
        SET_REG_FIELD(*pTevReg, 2, 16, bias);
    } else {
        SET_REG_FIELD(*pTevReg, 2, 20, (op >> 1) & 3);
        SET_REG_FIELD(*pTevReg, 2, 16, 3);
    }
    SET_REG_FIELD(*pTevReg, 1, 19, clamp & 0xFF);
    SET_REG_FIELD(*pTevReg, 2, 22, out_reg);

    GX_WRITE_RAS_REG(*pTevReg);
    gx->bpSentNot = 0;
}

void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, u8 clamp, u32 out_reg) {
    u32* pTevReg;

    pTevReg = &gx->teva[stage];
    SET_REG_FIELD(*pTevReg, 1, 18, op & 1);
    if (op <= 1) {
        SET_REG_FIELD(*pTevReg, 2, 20, scale);
        SET_REG_FIELD(*pTevReg, 2, 16, bias);
    } else {
        SET_REG_FIELD(*pTevReg, 2, 20, (op >> 1) & 3);
        SET_REG_FIELD(*pTevReg, 2, 16, 3);
    }
    SET_REG_FIELD(*pTevReg, 1, 19, clamp & 0xFF);
    SET_REG_FIELD(*pTevReg, 2, 22, out_reg);

    GX_WRITE_RAS_REG(*pTevReg);
    gx->bpSentNot = 0;
}

void GXSetTevKColor(u32 id, GXColor color) {
    u32 regRA;
    u32 regBG;

    regRA = 0;
    SET_REG_FIELD(regRA, 8, 0, color.r);
    SET_REG_FIELD(regRA, 8, 12, color.a);
    SET_REG_FIELD(regRA, 4, 20, 8);  // konst register
    SET_REG_FIELD(regRA, 8, 24, 0xE0 + id * 2);

    regBG = 0;
    SET_REG_FIELD(regBG, 8, 0, color.b);
    SET_REG_FIELD(regBG, 8, 12, color.g);
    SET_REG_FIELD(regBG, 4, 20, 8);
    SET_REG_FIELD(regBG, 8, 24, 0xE1 + id * 2);

    GX_WRITE_RAS_REG(regRA);
    GX_WRITE_RAS_REG(regBG);
    gx->bpSentNot = 0;
}

void GXSetTevKColorSel(u32 stage, u32 sel) {
    u32* Kreg;

    Kreg = &gx->tevKsel[stage >> 1];
    if (stage & 1) {
        SET_REG_FIELD(*Kreg, 5, 14, sel);
    } else {
        SET_REG_FIELD(*Kreg, 5, 4, sel);
    }

    GX_WRITE_RAS_REG(*Kreg);
    gx->bpSentNot = 0;
}

void GXSetTevKAlphaSel(u32 stage, u32 sel) {
    u32* Kreg;

    Kreg = &gx->tevKsel[stage >> 1];
    if (stage & 1) {
        SET_REG_FIELD(*Kreg, 5, 19, sel);
    } else {
        SET_REG_FIELD(*Kreg, 5, 9, sel);
    }

    GX_WRITE_RAS_REG(*Kreg);
    gx->bpSentNot = 0;
}

void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel) {
    u32* pTevReg;

    pTevReg = &gx->teva[stage];
    SET_REG_FIELD(*pTevReg, 2, 0, ras_sel);
    SET_REG_FIELD(*pTevReg, 2, 2, tex_sel);

    GX_WRITE_RAS_REG(*pTevReg);
    gx->bpSentNot = 0;
}

void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha) {
    u32* Kreg;

    Kreg = &gx->tevKsel[table << 1];
    SET_REG_FIELD(*Kreg, 2, 0, red);
    SET_REG_FIELD(*Kreg, 2, 2, green);
    GX_WRITE_RAS_REG(*Kreg);

    Kreg = &gx->tevKsel[(table << 1) + 1];
    SET_REG_FIELD(*Kreg, 2, 0, blue);
    SET_REG_FIELD(*Kreg, 2, 2, alpha);
    GX_WRITE_RAS_REG(*Kreg);

    gx->bpSentNot = 0;
}

void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1) {
    u32 reg;

    reg = 0;
    SET_REG_FIELD(reg, 8, 0, ref0);
    SET_REG_FIELD(reg, 8, 8, ref1);
    SET_REG_FIELD(reg, 3, 16, comp0);
    SET_REG_FIELD(reg, 3, 19, comp1);
    SET_REG_FIELD(reg, 2, 22, op);
    SET_REG_FIELD(reg, 8, 24, 0xF3);

    GX_WRITE_RAS_REG(reg);
    gx->bpSentNot = 0;
}

void GXSetZTexture(u32 op, u32 fmt, u32 bias) {
    u32 zenv0;
    u32 zenv1;
    u32 type;

    zenv0 = 0;
    SET_REG_FIELD(zenv0, 24, 0, bias);
    SET_REG_FIELD(zenv0, 8, 24, 0xF4);

    zenv1 = 0;
    switch (fmt) {
    case GX_TF_Z8:
        type = 0;
        break;
    case GX_TF_Z16:
        type = 1;
        break;
    case GX_TF_Z24X8:
        type = 2;
        break;
    default:
        type = 2;
        break;
    }

    SET_REG_FIELD(zenv1, 2, 0, type);
    SET_REG_FIELD(zenv1, 2, 2, op);
    SET_REG_FIELD(zenv1, 8, 24, 0xF5);

    GX_WRITE_RAS_REG(zenv0);
    GX_WRITE_RAS_REG(zenv1);
    gx->bpSentNot = 0;
}

void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color) {
    u32* ptref;
    u32 tmap;
    u32 tcoord;
    static int c2r[] = {0, 1, 0, 1, 0, 1, 7, 5, 6};  // GXChannelID to raster colour select

    ptref = &gx->tref[stage / 2];
    gx->texmapId[stage] = map;

    tmap = map & ~0x100;
    tmap = (tmap >= GX_MAX_TEXMAP) ? 0 : tmap;

    if (coord >= GX_MAX_TEXCOORD) {
        tcoord = 0;
        gx->tevTcEnab = gx->tevTcEnab & ~(1 << stage);
    } else {
        tcoord = coord;
        gx->tevTcEnab = gx->tevTcEnab | (1 << stage);
    }

    if (stage & 1) {
        SET_REG_FIELD(*ptref, 3, 12, tmap);
        SET_REG_FIELD(*ptref, 3, 15, tcoord);
        SET_REG_FIELD(*ptref, 3, 19, (color == GX_COLOR_NULL) ? 7 : c2r[color]);
        SET_REG_FIELD(*ptref, 1, 18, (map != GX_TEXMAP_NULL && !(map & 0x100)));
    } else {
        SET_REG_FIELD(*ptref, 3, 0, tmap);
        SET_REG_FIELD(*ptref, 3, 3, tcoord);
        SET_REG_FIELD(*ptref, 3, 7, (color == GX_COLOR_NULL) ? 7 : c2r[color]);
        SET_REG_FIELD(*ptref, 1, 6, (map != GX_TEXMAP_NULL && !(map & 0x100)));
    }

    GX_WRITE_RAS_REG(*ptref);
    gx->bpSentNot = 0;
    gx->dirtyState |= GX_DIRTY_SU_TEX;
}

void GXSetNumTevStages(u8 nStages) {
    SET_REG_FIELD(gx->genMode, 4, 10, nStages - 1);
    gx->dirtyState |= GX_DIRTY_GEN_MODE;
}
