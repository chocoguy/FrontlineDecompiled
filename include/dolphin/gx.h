// Dolphin SDK GX (graphics) declarations, added as decompiled code needs them.
#ifndef _DOLPHIN_GX_H_
#define _DOLPHIN_GX_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _GXProjectionType {
    GX_PERSPECTIVE,
    GX_ORTHOGRAPHIC,
} GXProjectionType;

#define GX_PNMTX0 0

typedef struct _GXRenderModeObj {
    /* 0x00 */ u32 viTVmode;
    /* 0x04 */ u16 fbWidth;
    /* 0x06 */ u16 efbHeight;
    /* 0x08 */ u16 xfbHeight;
    /* 0x0A */ u16 viXOrigin;
    /* 0x0C */ u16 viYOrigin;
    /* 0x0E */ u16 viWidth;
    /* 0x10 */ u16 viHeight;
    /* 0x14 */ u32 xFBmode;
    /* 0x18 */ u8 field_rendering;
    /* 0x19 */ u8 aa;
    /* 0x1A */ u8 sample_pattern[12][2];
    /* 0x32 */ u8 vfilter[7];
} GXRenderModeObj;  // size 0x3C

typedef struct _GXTexObj {
    u32 dummy[8];
} GXTexObj;

typedef struct _GXTlutObj {
    u32 dummy[3];
} GXTlutObj;

typedef struct _GXColor {
    u8 r, g, b, a;
} GXColor;

typedef struct _GXLightObj {
    u32 dummy[16];
} GXLightObj;

void GXDrawDone(void);

extern GXRenderModeObj GXNtsc480IntDf;
extern GXRenderModeObj GXPal528IntDf;
extern GXRenderModeObj GXMpal480IntDf;
void* GXInit(void* base, u32 size);  // returns the GXFifoObj
void GXAdjustForOverscan(const GXRenderModeObj* rmin, GXRenderModeObj* rmout, u16 hor, u16 ver);
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight);
u32 GXSetDispCopyYScale(f32 vscale);
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht);
void GXSetDispCopyDst(u16 wd, u16 ht);
void GXSetCopyFilter(u8 aa, const u8 sample_pattern[12][2], u8 vf, const u8 vfilter[7]);
void GXSetPixelFmt(u32 pix_fmt, u32 z_fmt);
void GXCopyDisp(void* dest, u8 clear);
void GXSetDispCopyGamma(u32 gamma);
void GXInvalidateVtxCache(void);
void GXSetColorUpdate(u8 update_enable);
void GXSetCopyClear(GXColor clear_clr, u32 clear_z);
void GXInitLightColor(GXLightObj* lt_obj, GXColor color);
void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz);
void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_brightness, u32 dist_func);
void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, u32 spot_func);

void GXInitTexObj(GXTexObj* obj, void* image, u16 width, u16 height, u32 format, u32 wrapS,
                  u32 wrapT, u8 mipmap);
void GXInitTexObjCI(GXTexObj* obj, void* image, u16 width, u16 height, u32 format, u32 wrapS,
                    u32 wrapT, u8 mipmap, u32 tlutName);
void GXInitTexObjLOD(GXTexObj* obj, u32 minFilt, u32 magFilt, f32 minLod, f32 maxLod, f32 lodBias,
                     u8 biasClamp, u8 doEdgeLod, u32 maxAniso);
void GXInitTlutObj(GXTlutObj* obj, void* lut, u32 format, u16 numEntries);
void GXLoadTexObj(GXTexObj* obj, u32 id);

// Texture object queries
u16 GXGetTexObjWidth(GXTexObj* obj);
u16 GXGetTexObjHeight(GXTexObj* obj);
u32 GXGetTexObjFmt(GXTexObj* obj);

// Indirect texturing and TEV swaps/konst colors
void GXSetIndTexCoordScale(u32 ind_stage, u32 scale_s, u32 scale_t);
void GXSetIndTexMtx(u32 mtx_id, f32 offset[2][3], s8 scale_exp);
void GXSetNumIndStages(u8 nIndStages);
void GXSetIndTexOrder(u32 ind_stage, u32 tex_coord, u32 tex_map);
void GXSetTevIndirect(u32 tev_stage, u32 ind_stage, u32 format, u32 bias_sel, u32 matrix_sel, u32 wrap_s,
                      u32 wrap_t, u8 add_prev, u8 utc_lod, u32 alpha_sel);
void GXSetTevIndRepeat(u32 tev_stage);
void GXSetTevDirect(u32 tev_stage);
volatile void* GXRedirectWriteGatherPipe(void* ptr);
void GXRestoreWriteGatherPipe(void);
void GXGetVtxAttrFmt(u32 vtxfmt, u32 attr, u32* cnt, u32* type, u8* frac);
void GXSetTevKColor(u32 id, GXColor color);
void GXSetTevKColorSel(u32 stage, u32 sel);
void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel);
void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha);
void GXLoadTlut(GXTlutObj* obj, u32 name);

void GXSetProjection(const f32 mtx[4][4], GXProjectionType type);
void GXGetProjectionv(f32* p);
void GXSetProjectionv(const f32* p);
void GXClearVtxDesc(void);
void GXSetVtxDesc(u32 attr, u32 type);
void GXSetVtxAttrFmt(u32 vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac);
void GXSetNumChans(u8 n);
void GXSetNumTexGens(u8 n);
void GXSetNumTevStages(u8 n);
void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color);
void GXSetTevOp(u32 stage, u32 mode);
void GXSetBlendMode(u32 type, u32 srcFactor, u32 dstFactor, u32 op);
void GXSetZMode(u8 compareEnable, u32 func, u8 updateEnable);
void GXSetCullMode(u32 mode);
void GXSetZCompLoc(u8 beforeTex);
void GXBegin(u32 type, u32 vtxfmt, u16 nverts);

// Vertex attributes, descriptor types and primitives used by game code.
#define GX_VA_POS 9
#define GX_VA_CLR0 11
#define GX_VA_TEX0 13
#define GX_DIRECT 1
#define GX_QUADS 0x80
#define GX_IDENTITY 60

// Write-gather pipe for immediate-mode vertex data.
typedef union {
    u8 u8;
    s16 s16;
    u16 u16;
    u32 u32;
    f32 f32;
} PPCWGPipe;
#define GXWGFifo (*(volatile PPCWGPipe*)0xCC008000)

static inline void GXPosition3s16(s16 x, s16 y, s16 z) {
    GXWGFifo.s16 = x;
    GXWGFifo.s16 = y;
    GXWGFifo.s16 = z;
}
static inline void GXPosition3f32(f32 x, f32 y, f32 z) {
    GXWGFifo.f32 = x;
    GXWGFifo.f32 = y;
    GXWGFifo.f32 = z;
}
static inline void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {
    GXWGFifo.u8 = r;
    GXWGFifo.u8 = g;
    GXWGFifo.u8 = b;
    GXWGFifo.u8 = a;
}
static inline void GXColor1u32(u32 clr) {
    GXWGFifo.u32 = clr;
}
static inline void GXTexCoord2f32(f32 s, f32 t) {
    GXWGFifo.f32 = s;
    GXWGFifo.f32 = t;
}
static inline void GXEnd(void) {}

void GXSetCurrentMtx(u32 id);
void GXSetChanCtrl(u32 chan, u8 enable, u32 amb_src, u32 mat_src, u32 light_mask, u32 diff_fn, u32 attn_fn);
void GXSetTexCoordGen2(u32 dst, u32 func, u32 src, u32 mtx, u8 normalize, u32 postMtx);
void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u8 clamp, u32 outReg);
void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, u8 clamp, u32 outReg);
void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1);

void PSMTXIdentity(f32 m[3][4]);
void PSMTXScale(f32 m[3][4], f32 xS, f32 yS, f32 zS);
void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, u32 type);
u32 PSMTXInvXpose(const f32 src[3][4], f32 invX[3][4]);
void PSMTXMultVec(const f32 m[3][4], const f32* src, f32* dst);  // Vec* in the SDK
void GXLoadNrmMtxImm(const f32 mtx[3][4], u32 id);
void GXCallDisplayList(void* list, u32 nbytes);
void GXLoadPosMtxIndx(u16 mtx_indx, u32 id);
void GXSetArray(u32 attr, const void* base_ptr, u8 stride);
void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z);
void GXLoadLightObjImm(GXLightObj* lt_obj, u32 light);
void GXSetChanAmbColor(u32 chan, GXColor amb_color);
void GXSetChanMatColor(u32 chan, GXColor mat_color);
void GXSetFog(u32 type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color);
void C_MTXOrtho(f32 m[4][4], f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void GXLoadPosMtxImm(const f32 mtx[3][4], u32 id);
void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);
void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field);
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);

#ifdef __cplusplus
}
#endif

#endif
