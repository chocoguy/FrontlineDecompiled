#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>
#include <math.h>

// GXSpotFn
#define GX_SP_OFF 0
#define GX_SP_FLAT 1
#define GX_SP_COS 2
#define GX_SP_COS2 3
#define GX_SP_SHARP 4
#define GX_SP_RING1 5
#define GX_SP_RING2 6

// GXDistAttnFn
#define GX_DA_OFF 0
#define GX_DA_GENTLE 1
#define GX_DA_MEDIUM 2
#define GX_DA_STEEP 3

// GXChannelID
#define GX_COLOR0 0
#define GX_COLOR1 1
#define GX_ALPHA0 2
#define GX_ALPHA1 3
#define GX_COLOR0A0 4
#define GX_COLOR1A1 5

// GXLightID
#define GX_LIGHT0 0x001
#define GX_LIGHT1 0x002
#define GX_LIGHT2 0x004
#define GX_LIGHT3 0x008
#define GX_LIGHT4 0x010
#define GX_LIGHT5 0x020
#define GX_LIGHT6 0x040
#define GX_LIGHT7 0x080

#define GX_DF_NONE 0
#define GX_AF_SPEC 0
#define GX_AF_NONE 2

void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, u32 spot_func) {
    f32 a0;
    f32 a1;
    f32 a2;
    f32 d;
    f32 cr;
    __GXLightObjInt* obj = (__GXLightObjInt*)lt_obj;

    if (cutoff <= 0.0f || cutoff > 90.0f) {
        spot_func = GX_SP_OFF;
    }

    cr = cosf((3.1415927f * cutoff) / 180.0f);

    switch (spot_func) {
    case GX_SP_FLAT:
        a0 = -1000.0f * cr;
        a1 = 1000.0f;
        a2 = 0.0f;
        break;
    case GX_SP_COS:
        a0 = -cr / (1.0f - cr);
        a1 = 1.0f / (1.0f - cr);
        a2 = 0.0f;
        break;
    case GX_SP_COS2:
        a0 = 0.0f;
        a1 = -cr / (1.0f - cr);
        a2 = 1.0f / (1.0f - cr);
        break;
    case GX_SP_SHARP:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = (cr * (cr - 2.0f)) / d;
        a1 = 2.0f / d;
        a2 = -1.0f / d;
        break;
    case GX_SP_RING1:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = (-4.0f * cr) / d;
        a1 = (4.0f * (1.0f + cr)) / d;
        a2 = -4.0f / d;
        break;
    case GX_SP_RING2:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = 1.0f - ((2.0f * cr * cr) / d);
        a1 = (4.0f * cr) / d;
        a2 = -2.0f / d;
        break;
    case GX_SP_OFF:
    default:
        a0 = 1.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        break;
    }

    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}

void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br, u32 dist_func) {
    f32 k0;
    f32 k1;
    f32 k2;
    __GXLightObjInt* obj = (__GXLightObjInt*)lt_obj;

    if (ref_dist < 0.0f) {
        dist_func = GX_DA_OFF;
    }
    if (ref_br <= 0.0f || ref_br >= 1.0f) {
        dist_func = GX_DA_OFF;
    }

    switch (dist_func) {
    case GX_DA_GENTLE:
        k0 = 1.0f;
        k1 = (1.0f - ref_br) / (ref_br * ref_dist);
        k2 = 0.0f;
        break;
    case GX_DA_MEDIUM:
        k0 = 1.0f;
        k1 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist);
        k2 = (0.5f * (1.0f - ref_br)) / (ref_dist * (ref_br * ref_dist));
        break;
    case GX_DA_STEEP:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = (1.0f - ref_br) / (ref_dist * (ref_br * ref_dist));
        break;
    case GX_DA_OFF:
    default:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = 0.0f;
        break;
    }

    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z) {
    __GXLightObjInt* obj = (__GXLightObjInt*)lt_obj;

    obj->lpos[0] = x;
    obj->lpos[1] = y;
    obj->lpos[2] = z;
}

void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
    __GXLightObjInt* obj = (__GXLightObjInt*)lt_obj;

    obj->ldir[0] = -nx;
    obj->ldir[1] = -ny;
    obj->ldir[2] = -nz;
}

void GXInitLightColor(GXLightObj* lt_obj, GXColor color) {
    __GXLightObjInt* obj = (__GXLightObjInt*)lt_obj;

    obj->Color = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
}

void GXLoadLightObjImm(GXLightObj* lt_obj, u32 light) {
    u32 addr;
    u32 idx;
    __GXLightObjInt* obj = (__GXLightObjInt*)lt_obj;

    switch (light) {
    case GX_LIGHT0:
        idx = 0;
        break;
    case GX_LIGHT1:
        idx = 1;
        break;
    case GX_LIGHT2:
        idx = 2;
        break;
    case GX_LIGHT3:
        idx = 3;
        break;
    case GX_LIGHT4:
        idx = 4;
        break;
    case GX_LIGHT5:
        idx = 5;
        break;
    case GX_LIGHT6:
        idx = 6;
        break;
    case GX_LIGHT7:
        idx = 7;
        break;
    default:
        idx = 0;
        break;
    }

    // XF light memory: 16 words per light, written as one 16-word block.
    addr = idx * 0x10 + 0x600;
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(addr | (0xF << 16));

    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(obj->Color);
    GX_WRITE_F32(obj->a[0]);
    GX_WRITE_F32(obj->a[1]);
    GX_WRITE_F32(obj->a[2]);
    GX_WRITE_F32(obj->k[0]);
    GX_WRITE_F32(obj->k[1]);
    GX_WRITE_F32(obj->k[2]);
    GX_WRITE_F32(obj->lpos[0]);
    GX_WRITE_F32(obj->lpos[1]);
    GX_WRITE_F32(obj->lpos[2]);
    GX_WRITE_F32(obj->ldir[0]);
    GX_WRITE_F32(obj->ldir[1]);
    GX_WRITE_F32(obj->ldir[2]);

    gx->bpSentNot = 1;
}

void GXSetChanAmbColor(u32 chan, GXColor amb_color) {
    u32 reg;
    u32 colIdx;
    u32 alpha;

    switch (chan) {
    case GX_COLOR0:
        alpha = gx->ambColor[0] & 0xFF;
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, amb_color.r);
        SET_REG_FIELD(reg, 8, 16, amb_color.g);
        SET_REG_FIELD(reg, 8, 8, amb_color.b);
        SET_REG_FIELD(reg, 8, 0, alpha);
        colIdx = 0;
        break;
    case GX_COLOR1:
        alpha = gx->ambColor[1] & 0xFF;
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, amb_color.r);
        SET_REG_FIELD(reg, 8, 16, amb_color.g);
        SET_REG_FIELD(reg, 8, 8, amb_color.b);
        SET_REG_FIELD(reg, 8, 0, alpha);
        colIdx = 1;
        break;
    case GX_ALPHA0:
        reg = gx->ambColor[0];
        SET_REG_FIELD(reg, 8, 0, amb_color.a);
        colIdx = 0;
        break;
    case GX_ALPHA1:
        reg = gx->ambColor[1];
        SET_REG_FIELD(reg, 8, 0, amb_color.a);
        colIdx = 1;
        break;
    case GX_COLOR0A0:
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, amb_color.r);
        SET_REG_FIELD(reg, 8, 16, amb_color.g);
        SET_REG_FIELD(reg, 8, 8, amb_color.b);
        SET_REG_FIELD(reg, 8, 0, amb_color.a);
        colIdx = 0;
        break;
    case GX_COLOR1A1:
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, amb_color.r);
        SET_REG_FIELD(reg, 8, 16, amb_color.g);
        SET_REG_FIELD(reg, 8, 8, amb_color.b);
        SET_REG_FIELD(reg, 8, 0, amb_color.a);
        colIdx = 1;
        break;
    default:
        return;
    }

    GX_WRITE_XF_REG(colIdx + 0xA, reg);
    gx->bpSentNot = 1;
    gx->ambColor[colIdx] = reg;
}

void GXSetChanMatColor(u32 chan, GXColor mat_color) {
    u32 reg;
    u32 colIdx;
    u32 alpha;

    switch (chan) {
    case GX_COLOR0:
        alpha = gx->matColor[0] & 0xFF;
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, mat_color.r);
        SET_REG_FIELD(reg, 8, 16, mat_color.g);
        SET_REG_FIELD(reg, 8, 8, mat_color.b);
        SET_REG_FIELD(reg, 8, 0, alpha);
        colIdx = 0;
        break;
    case GX_COLOR1:
        alpha = gx->matColor[1] & 0xFF;
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, mat_color.r);
        SET_REG_FIELD(reg, 8, 16, mat_color.g);
        SET_REG_FIELD(reg, 8, 8, mat_color.b);
        SET_REG_FIELD(reg, 8, 0, alpha);
        colIdx = 1;
        break;
    case GX_ALPHA0:
        reg = gx->matColor[0];
        SET_REG_FIELD(reg, 8, 0, mat_color.a);
        colIdx = 0;
        break;
    case GX_ALPHA1:
        reg = gx->matColor[1];
        SET_REG_FIELD(reg, 8, 0, mat_color.a);
        colIdx = 1;
        break;
    case GX_COLOR0A0:
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, mat_color.r);
        SET_REG_FIELD(reg, 8, 16, mat_color.g);
        SET_REG_FIELD(reg, 8, 8, mat_color.b);
        SET_REG_FIELD(reg, 8, 0, mat_color.a);
        colIdx = 0;
        break;
    case GX_COLOR1A1:
        reg = 0;
        SET_REG_FIELD(reg, 8, 24, mat_color.r);
        SET_REG_FIELD(reg, 8, 16, mat_color.g);
        SET_REG_FIELD(reg, 8, 8, mat_color.b);
        SET_REG_FIELD(reg, 8, 0, mat_color.a);
        colIdx = 1;
        break;
    default:
        return;
    }

    GX_WRITE_XF_REG(colIdx + 0xC, reg);
    gx->bpSentNot = 1;
    gx->matColor[colIdx] = reg;
}

void GXSetNumChans(u8 nChans) {
    SET_REG_FIELD(gx->genMode, 3, 4, nChans);
    GX_WRITE_XF_REG(0x09, nChans);
    gx->dirtyState |= GX_DIRTY_GEN_MODE;
}

void GXSetChanCtrl(u32 chan, u8 enable, u32 amb_src, u32 mat_src, u32 light_mask, u32 diff_fn, u32 attn_fn) {
    u32 reg;
    u32 idx;

    if (chan == GX_COLOR0A0) {
        idx = 0;
    } else if (chan == GX_COLOR1A1) {
        idx = 1;
    } else {
        idx = chan;
    }

    reg = 0;
    SET_REG_FIELD(reg, 1, 1, enable);
    SET_REG_FIELD(reg, 1, 0, mat_src);
    SET_REG_FIELD(reg, 1, 6, amb_src);
    SET_REG_FIELD(reg, 1, 2, (light_mask & GX_LIGHT0) != 0);
    SET_REG_FIELD(reg, 1, 3, (light_mask & GX_LIGHT1) != 0);
    SET_REG_FIELD(reg, 1, 4, (light_mask & GX_LIGHT2) != 0);
    SET_REG_FIELD(reg, 1, 5, (light_mask & GX_LIGHT3) != 0);
    SET_REG_FIELD(reg, 1, 11, (light_mask & GX_LIGHT4) != 0);
    SET_REG_FIELD(reg, 1, 12, (light_mask & GX_LIGHT5) != 0);
    SET_REG_FIELD(reg, 1, 13, (light_mask & GX_LIGHT6) != 0);
    SET_REG_FIELD(reg, 1, 14, (light_mask & GX_LIGHT7) != 0);
    SET_REG_FIELD(reg, 2, 7, (attn_fn == GX_AF_SPEC) ? GX_DF_NONE : diff_fn);
    SET_REG_FIELD(reg, 1, 9, (attn_fn != GX_AF_NONE));
    SET_REG_FIELD(reg, 1, 10, (attn_fn != GX_AF_SPEC));

    GX_WRITE_XF_REG(idx + 0xE, reg);
    gx->bpSentNot = 1;

    // The colour+alpha channel IDs also set the matching alpha channel.
    if (chan == GX_COLOR0A0) {
        GX_WRITE_XF_REG(0x10, reg);
    } else if (chan == GX_COLOR1A1) {
        GX_WRITE_XF_REG(0x11, reg);
    }
}
