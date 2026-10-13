#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>
#include <string.h>

// GXTexFmt
#define GX_TF_I4 0x0
#define GX_TF_I8 0x1
#define GX_TF_IA4 0x2
#define GX_TF_IA8 0x3
#define GX_TF_RGB565 0x4
#define GX_TF_RGB5A3 0x5
#define GX_TF_RGBA8 0x6
#define GX_TF_C4 0x8
#define GX_TF_C8 0x9
#define GX_TF_C14X2 0xA
#define GX_TF_CMPR 0xE

#define GX_LINEAR 1

// GXTexCacheSize
#define GX_TEXCACHE_32K 0
#define GX_TEXCACHE_128K 1
#define GX_TEXCACHE_512K 2
#define GX_TEXCACHE_NONE 3

// BP register IDs per texture map: maps 0-3 then 4-7.
u8 GXTexMode0Ids[8] = {0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3};
u8 GXTexMode1Ids[8] = {0x84, 0x85, 0x86, 0x87, 0xA4, 0xA5, 0xA6, 0xA7};
u8 GXTexImage0Ids[8] = {0x88, 0x89, 0x8A, 0x8B, 0xA8, 0xA9, 0xAA, 0xAB};
u8 GXTexImage1Ids[8] = {0x8C, 0x8D, 0x8E, 0x8F, 0xAC, 0xAD, 0xAE, 0xAF};
u8 GXTexImage2Ids[8] = {0x90, 0x91, 0x92, 0x93, 0xB0, 0xB1, 0xB2, 0xB3};
u8 GXTexImage3Ids[8] = {0x94, 0x95, 0x96, 0x97, 0xB4, 0xB5, 0xB6, 0xB7};
u8 GXTexTlutIds[8] = {0x98, 0x99, 0x9A, 0x9B, 0xB8, 0xB9, 0xBA, 0xBB};

// GXTexFilter to the hardware's min-filter encoding.
u8 GX2HWFiltConv[6] = {0x00, 0x04, 0x01, 0x05, 0x02, 0x06};

void GXInitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t,
                  u8 mipmap) {
    u32 imageBase;
    u16 maxLOD;
    u16 rowT;
    u16 colT;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    memset(t, 0, 0x20);

    SET_REG_FIELD(t->mode0, 2, 0, wrap_s);
    SET_REG_FIELD(t->mode0, 2, 2, wrap_t);
    SET_REG_FIELD(t->mode0, 1, 4, 1);

    if (mipmap != 0) {
        u8 lmax;

        t->flags |= 1;
        if (format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2) {
            SET_REG_FIELD(t->mode0, 3, 5, 5);
        } else {
            SET_REG_FIELD(t->mode0, 3, 5, 6);
        }

        if (width > height) {
            maxLOD = 31 - __cntlzw(width);
        } else {
            maxLOD = 31 - __cntlzw(height);
        }

        lmax = 16.0f * maxLOD;
        SET_REG_FIELD(t->mode1, 8, 8, lmax);
    } else {
        SET_REG_FIELD(t->mode0, 3, 5, 4);
    }

    t->fmt = format;
    SET_REG_FIELD(t->image0, 10, 0, width - 1);
    SET_REG_FIELD(t->image0, 10, 10, height - 1);
    SET_REG_FIELD(t->image0, 4, 20, format & 0xF);

    imageBase = ((u32)image_ptr & 0x3FFFFFFF) >> 5;
    SET_REG_FIELD(t->image3, 21, 0, imageBase);

    // TMEM load format and tile size (log2 of texels per tile edge).
    switch (format & 0xF) {
    case GX_TF_I4:
    case GX_TF_C4:
        t->loadFmt = 1;
        rowT = 3;
        colT = 3;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_C8:
        t->loadFmt = 2;
        rowT = 3;
        colT = 2;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_C14X2:
        t->loadFmt = 2;
        rowT = 2;
        colT = 2;
        break;
    case GX_TF_RGBA8:
        t->loadFmt = 3;
        rowT = 2;
        colT = 2;
        break;
    case GX_TF_CMPR:
        t->loadFmt = 0;
        rowT = 3;
        colT = 3;
        break;
    default:
        t->loadFmt = 2;
        rowT = 2;
        colT = 2;
        break;
    }

    t->loadCnt = (((width + (1 << rowT) - 1) >> rowT) * ((height + (1 << colT) - 1) >> colT)) & 0x7FFF;
    t->flags |= 2;
}

void GXInitTexObjCI(GXTexObj* obj, void* image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t,
                    u8 mipmap, u32 tlut_name) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    GXInitTexObj(obj, image_ptr, width, height, format, wrap_s, wrap_t, mipmap);
    t->flags &= ~2;
    t->tlutName = tlut_name;
}

void GXInitTexObjLOD(GXTexObj* obj, u32 min_filt, u32 mag_filt, f32 min_lod, f32 max_lod, f32 lod_bias,
                     u8 bias_clamp, u8 do_edge_lod, u32 max_aniso) {
    u8 lbias;
    u8 lmin;
    u8 lmax;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    if (lod_bias < -4.0f) {
        lod_bias = -4.0f;
    } else if (lod_bias >= 4.0f) {
        lod_bias = 3.99f;
    }

    lbias = 32.0f * lod_bias;
    SET_REG_FIELD(t->mode0, 8, 9, lbias);
    SET_REG_FIELD(t->mode0, 1, 4, (mag_filt == GX_LINEAR) ? 1 : 0);
    SET_REG_FIELD(t->mode0, 3, 5, GX2HWFiltConv[min_filt]);
    SET_REG_FIELD(t->mode0, 1, 8, do_edge_lod ? 0 : 1);
    SET_REG_FIELD(t->mode0, 1, 17, 0);
    SET_REG_FIELD(t->mode0, 1, 18, 0);
    SET_REG_FIELD(t->mode0, 2, 19, max_aniso);
    SET_REG_FIELD(t->mode0, 1, 21, bias_clamp);

    if (min_lod < 0.0f) {
        min_lod = 0.0f;
    } else if (min_lod > 10.0f) {
        min_lod = 10.0f;
    }
    lmin = 16.0f * min_lod;

    if (max_lod < 0.0f) {
        max_lod = 0.0f;
    } else if (max_lod > 10.0f) {
        max_lod = 10.0f;
    }
    lmax = 16.0f * max_lod;

    SET_REG_FIELD(t->mode1, 8, 0, lmin);
    SET_REG_FIELD(t->mode1, 8, 8, lmax);
}

u16 GXGetTexObjWidth(GXTexObj* obj) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    return (u32)GET_REG_FIELD(t->image0, 10, 0) + 1;
}

u16 GXGetTexObjHeight(GXTexObj* obj) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    return (u32)GET_REG_FIELD(t->image0, 10, 10) + 1;
}

u32 GXGetTexObjFmt(GXTexObj* obj) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    return t->fmt;
}

void GXLoadTexObjPreLoaded(GXTexObj* obj, GXTexRegion* region, u32 id) {
    __GXTlutRegionInt* tlr;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;
    __GXTexRegionInt* r = (__GXTexRegionInt*)region;

    SET_REG_FIELD(t->mode0, 8, 24, GXTexMode0Ids[id]);
    SET_REG_FIELD(t->mode1, 8, 24, GXTexMode1Ids[id]);
    SET_REG_FIELD(t->image0, 8, 24, GXTexImage0Ids[id]);
    SET_REG_FIELD(r->image1, 8, 24, GXTexImage1Ids[id]);
    SET_REG_FIELD(r->image2, 8, 24, GXTexImage2Ids[id]);
    SET_REG_FIELD(t->image3, 8, 24, GXTexImage3Ids[id]);

    GX_WRITE_RAS_REG(t->mode0);
    GX_WRITE_RAS_REG(t->mode1);
    GX_WRITE_RAS_REG(t->image0);
    GX_WRITE_RAS_REG(r->image1);
    GX_WRITE_RAS_REG(r->image2);
    GX_WRITE_RAS_REG(t->image3);

    if (!(t->flags & 2)) {
        // Colour-index texture: point the map at its TLUT.
        tlr = (__GXTlutRegionInt*)gx->tlutRegionCallback(t->tlutName);
        SET_REG_FIELD(tlr->tlutObj.tlut, 8, 24, GXTexTlutIds[id]);
        GX_WRITE_RAS_REG(tlr->tlutObj.tlut);
    }

    gx->tImage0[id] = t->image0;
    gx->tMode0[id] = t->mode0;
    gx->dirtyState |= GX_DIRTY_SU_TEX;
    gx->bpSentNot = 0;
}

void GXLoadTexObj(GXTexObj* obj, u32 id) {
    GXTexRegion* r;

    r = gx->texRegionCallback(obj, id);
    GXLoadTexObjPreLoaded(obj, r, id);
}

void GXInitTlutObj(GXTlutObj* tlut_obj, void* lut, u32 fmt, u16 n_entries) {
    __GXTlutObjInt* t = (__GXTlutObjInt*)tlut_obj;

    t->tlut = 0;
    SET_REG_FIELD(t->tlut, 2, 10, fmt);
    SET_REG_FIELD(t->loadTlut0, 21, 0, ((u32)lut & 0x3FFFFFFF) >> 5);
    SET_REG_FIELD(t->loadTlut0, 8, 24, 0x64);
    t->numEntries = n_entries;
}

void GXLoadTlut(GXTlutObj* tlut_obj, u32 tlut_name) {
    __GXTlutRegionInt* r;
    u32 tlut_offset;
    __GXTlutObjInt* t = (__GXTlutObjInt*)tlut_obj;

    r = (__GXTlutRegionInt*)gx->tlutRegionCallback(tlut_name);

    __GXFlushTextureState();
    GX_WRITE_RAS_REG(t->loadTlut0);
    GX_WRITE_RAS_REG(r->loadTlut1);
    __GXFlushTextureState();

    tlut_offset = r->loadTlut1 & 0x3FF;
    SET_REG_FIELD(t->tlut, 10, 0, tlut_offset);
    r->tlutObj = *t;
}

void GXInitTexCacheRegion(GXTexRegion* region, u8 is_32b_mipmap, u32 tmem_even, u32 size_even, u32 tmem_odd,
                          u32 size_odd) {
    u32 WidthExp2;
    __GXTexRegionInt* t = (__GXTexRegionInt*)region;

    switch (size_even) {
    case GX_TEXCACHE_32K:
        WidthExp2 = 3;
        break;
    case GX_TEXCACHE_128K:
        WidthExp2 = 4;
        break;
    case GX_TEXCACHE_512K:
        WidthExp2 = 5;
        break;
    default:
        break;
    }

    t->image1 = 0;
    SET_REG_FIELD(t->image1, 15, 0, tmem_even >> 5);
    SET_REG_FIELD(t->image1, 3, 15, WidthExp2);
    SET_REG_FIELD(t->image1, 3, 18, WidthExp2);
    SET_REG_FIELD(t->image1, 1, 21, 0);

    switch (size_odd) {
    case GX_TEXCACHE_32K:
        WidthExp2 = 3;
        break;
    case GX_TEXCACHE_128K:
        WidthExp2 = 4;
        break;
    case GX_TEXCACHE_512K:
        WidthExp2 = 5;
        break;
    case GX_TEXCACHE_NONE:
        WidthExp2 = 0;
        break;
    default:
        break;
    }

    t->image2 = 0;
    SET_REG_FIELD(t->image2, 15, 0, tmem_odd >> 5);
    SET_REG_FIELD(t->image2, 3, 15, WidthExp2);
    SET_REG_FIELD(t->image2, 3, 18, WidthExp2);

    t->is32bMipmap = is_32b_mipmap;
    t->isCached = 1;
}

void GXInitTlutRegion(GXTlutRegion* region, u32 tmem_addr, u32 tlut_size) {
    __GXTlutRegionInt* t = (__GXTlutRegionInt*)region;

    t->loadTlut1 = 0;
    tmem_addr -= 0x80000;
    SET_REG_FIELD(t->loadTlut1, 10, 0, tmem_addr >> 9);
    SET_REG_FIELD(t->loadTlut1, 11, 10, tlut_size);
    SET_REG_FIELD(t->loadTlut1, 8, 24, 0x65);
}

void GXInvalidateTexAll(void) {
    u32 reg0;
    u32 reg1;

    reg0 = 0x66001000;
    reg1 = 0x66001100;
    __GXFlushTextureState();
    GX_WRITE_RAS_REG(reg0);
    GX_WRITE_RAS_REG(reg1);
    __GXFlushTextureState();
}

GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback f) {
    GXTexRegionCallback oldcb;

    oldcb = gx->texRegionCallback;
    gx->texRegionCallback = f;
    return oldcb;
}

GXTlutRegionCallback GXSetTlutRegionCallback(GXTlutRegionCallback f) {
    GXTlutRegionCallback oldcb;

    oldcb = gx->tlutRegionCallback;
    gx->tlutRegionCallback = f;
    return oldcb;
}

// Texture coordinate scale for normalised coordinates comes from the map size.
#pragma auto_inline off
static void __SetSURegs(u32 tmap, u32 tcoord) {
    u32 w;
    u32 h;
    u8 s_bias;
    u8 t_bias;

    w = GET_REG_FIELD(gx->tImage0[tmap], 10, 0);
    h = GET_REG_FIELD(gx->tImage0[tmap], 10, 10);
    SET_REG_FIELD(gx->suTs0[tcoord], 16, 0, w);
    SET_REG_FIELD(gx->suTs1[tcoord], 16, 0, h);

    s_bias = GET_REG_FIELD(gx->tMode0[tmap], 2, 0) == 1;
    t_bias = GET_REG_FIELD(gx->tMode0[tmap], 2, 2) == 1;
    SET_REG_FIELD(gx->suTs0[tcoord], 1, 16, s_bias);
    SET_REG_FIELD(gx->suTs1[tcoord], 1, 16, t_bias);

    GX_WRITE_RAS_REG(gx->suTs0[tcoord]);
    GX_WRITE_RAS_REG(gx->suTs1[tcoord]);
    gx->bpSentNot = 0;
}
#pragma auto_inline reset

void __GXSetSUTexRegs(void) {
    u32 nStages;
    u32 nIndStages;
    u32 i;
    u32 map;
    u32 tmap;
    u32 coord;
    u32* ptref;

    if (gx->tcsManEnab != 0xFF) {
        nStages = GET_REG_FIELD(gx->genMode, 4, 10) + 1;
        nIndStages = GET_REG_FIELD(gx->genMode, 3, 16);

        for (i = 0; i < nIndStages; i++) {
            switch (i) {
            case 0:
                tmap = GET_REG_FIELD(gx->iref, 3, 0);
                coord = GET_REG_FIELD(gx->iref, 3, 3);
                break;
            case 1:
                tmap = GET_REG_FIELD(gx->iref, 3, 6);
                coord = GET_REG_FIELD(gx->iref, 3, 9);
                break;
            case 2:
                tmap = GET_REG_FIELD(gx->iref, 3, 12);
                coord = GET_REG_FIELD(gx->iref, 3, 15);
                break;
            case 3:
                tmap = GET_REG_FIELD(gx->iref, 3, 18);
                coord = GET_REG_FIELD(gx->iref, 3, 21);
                break;
            }

            if (!(gx->tcsManEnab & (1 << coord))) {
                __SetSURegs(tmap, coord);
            }
        }

        for (i = 0; i < nStages; i++) {
            ptref = &gx->tref[i / 2];
            map = gx->texmapId[i];
            tmap = map & ~0x100;

            if (i & 1) {
                coord = GET_REG_FIELD(*ptref, 3, 15);
            } else {
                coord = GET_REG_FIELD(*ptref, 3, 3);
            }

            if (tmap != 0xFF && !(gx->tcsManEnab & (1 << coord)) && (gx->tevTcEnab & (1 << i))) {
                __SetSURegs(tmap, coord);
            }
        }
    }
}

// Splits TMEM between the even/odd cache banks: 0 = default, 1 = large caches.
void __GXSetTmemConfig(u32 config) {
    switch (config) {
    case 1:
        GX_WRITE_RAS_REG(0x8C0D8000);
        GX_WRITE_RAS_REG(0x900DC000);
        GX_WRITE_RAS_REG(0x8D0D8800);
        GX_WRITE_RAS_REG(0x910DC800);
        GX_WRITE_RAS_REG(0x8E0D9000);
        GX_WRITE_RAS_REG(0x920DD000);
        GX_WRITE_RAS_REG(0x8F0D9800);
        GX_WRITE_RAS_REG(0x930DD800);
        GX_WRITE_RAS_REG(0xAC0DA000);
        GX_WRITE_RAS_REG(0xB00DE000);
        GX_WRITE_RAS_REG(0xAD0DA800);
        GX_WRITE_RAS_REG(0xB10DE800);
        GX_WRITE_RAS_REG(0xAE0DB000);
        GX_WRITE_RAS_REG(0xB20DF000);
        GX_WRITE_RAS_REG(0xAF0DB800);
        GX_WRITE_RAS_REG(0xB30DF800);
        break;
    case 0:
    default:
        GX_WRITE_RAS_REG(0x8C0D8000);
        GX_WRITE_RAS_REG(0x900DC000);
        GX_WRITE_RAS_REG(0x8D0D8400);
        GX_WRITE_RAS_REG(0x910DC400);
        GX_WRITE_RAS_REG(0x8E0D8800);
        GX_WRITE_RAS_REG(0x920DC800);
        GX_WRITE_RAS_REG(0x8F0D8C00);
        GX_WRITE_RAS_REG(0x930DCC00);
        GX_WRITE_RAS_REG(0xAC0D9000);
        GX_WRITE_RAS_REG(0xB00DD000);
        GX_WRITE_RAS_REG(0xAD0D9400);
        GX_WRITE_RAS_REG(0xB10DD400);
        GX_WRITE_RAS_REG(0xAE0D9800);
        GX_WRITE_RAS_REG(0xB20DD800);
        GX_WRITE_RAS_REG(0xAF0D9C00);
        GX_WRITE_RAS_REG(0xB30DDC00);
        break;
    }
}
