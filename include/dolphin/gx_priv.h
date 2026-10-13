// Dolphin SDK GX internals shared between the gx/ units (SDK Dec 2001).
#ifndef _DOLPHIN_GX_PRIV_H_
#define _DOLPHIN_GX_PRIV_H_

#include <dolphin/gx.h>
#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef u8 GXBool;

typedef struct _GXTexRegion {
    u32 dummy[4];
} GXTexRegion;

typedef struct _GXTlutRegion {
    u32 dummy[4];
} GXTlutRegion;

typedef struct _GXFifoObj {
    u8 pad[128];
} GXFifoObj;

typedef GXTexRegion* (*GXTexRegionCallback)(GXTexObj* t_obj, u32 id);
typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 idx);
typedef void (*GXBreakPtCallback)(void);
typedef void (*GXDrawSyncCallback)(u16 token);
typedef void (*GXDrawDoneCallback)(void);

// Private layouts of the public opaque objects.
typedef struct __GXTexObjInt {
    /* 0x00 */ u32 mode0;
    /* 0x04 */ u32 mode1;
    /* 0x08 */ u32 image0;
    /* 0x0C */ u32 image3;
    /* 0x10 */ void* userData;
    /* 0x14 */ u32 fmt;
    /* 0x18 */ u32 tlutName;
    /* 0x1C */ u16 loadCnt;
    /* 0x1E */ u8 loadFmt;
    /* 0x1F */ u8 flags;
} __GXTexObjInt;

typedef struct __GXTexRegionInt {
    /* 0x00 */ u32 image1;
    /* 0x04 */ u32 image2;
    /* 0x08 */ u16 sizeEven;
    /* 0x0A */ u16 sizeOdd;
    /* 0x0C */ u8 is32bMipmap;
    /* 0x0D */ u8 isCached;
} __GXTexRegionInt;

typedef struct __GXTlutObjInt {
    /* 0x00 */ u32 tlut;
    /* 0x04 */ u32 loadTlut0;
    /* 0x08 */ u16 numEntries;
} __GXTlutObjInt;

typedef struct __GXTlutRegionInt {
    /* 0x00 */ u32 loadTlut1;
    /* 0x04 */ __GXTlutObjInt tlutObj;
} __GXTlutRegionInt;

typedef struct __GXLightObjInt {
    /* 0x00 */ u32 reserved[3];
    /* 0x0C */ u32 Color;
    /* 0x10 */ f32 a[3];
    /* 0x1C */ f32 k[3];
    /* 0x28 */ f32 lpos[3];
    /* 0x34 */ f32 ldir[3];
} __GXLightObjInt;

typedef struct __GXFifoObjInt {
    /* 0x00 */ u8* base;
    /* 0x04 */ u8* top;
    /* 0x08 */ u32 size;
    /* 0x0C */ u32 hiWatermark;
    /* 0x10 */ u32 loWatermark;
    /* 0x14 */ void* rdPtr;
    /* 0x18 */ void* wrPtr;
    /* 0x1C */ s32 count;
    /* 0x20 */ u8 bind_cpu;
    /* 0x21 */ u8 bind_gp;
} __GXFifoObjInt;

// All GX state shadowed in main memory. size 0x4F8
typedef struct _GXData {
    /* 0x000 */ u16 vNumNot;  // zero once vertices have been sent
    /* 0x002 */ u16 bpSentNot;  // zero once a BP (RAS) register has been written
    /* 0x004 */ u16 vNum;
    /* 0x006 */ u16 vLim;
    /* 0x008 */ u32 cpEnable;
    /* 0x00C */ u32 cpStatus;
    /* 0x010 */ u32 cpClr;
    /* 0x014 */ u32 vcdLo;
    /* 0x018 */ u32 vcdHi;
    /* 0x01C */ u32 vatA[8];
    /* 0x03C */ u32 vatB[8];
    /* 0x05C */ u32 vatC[8];
    /* 0x07C */ u32 lpSize;
    /* 0x080 */ u32 matIdxA;
    /* 0x084 */ u32 matIdxB;
    /* 0x088 */ u32 indexBase[4];
    /* 0x098 */ u32 indexStride[4];
    /* 0x0A8 */ u32 ambColor[2];
    /* 0x0B0 */ u32 matColor[2];
    /* 0x0B8 */ u32 suTs0[8];
    /* 0x0D8 */ u32 suTs1[8];
    /* 0x0F8 */ u32 suScis0;
    /* 0x0FC */ u32 suScis1;
    /* 0x100 */ u32 tref[8];
    /* 0x120 */ u32 iref;
    /* 0x124 */ u32 bpMask;
    /* 0x128 */ u32 IndTexScale0;
    /* 0x12C */ u32 IndTexScale1;
    /* 0x130 */ u32 tevc[16];
    /* 0x170 */ u32 teva[16];
    /* 0x1B0 */ u32 tevKsel[8];
    /* 0x1D0 */ u32 cmode0;
    /* 0x1D4 */ u32 cmode1;
    /* 0x1D8 */ u32 zmode;
    /* 0x1DC */ u32 peCtrl;
    /* 0x1E0 */ u32 cpDispSrc;
    /* 0x1E4 */ u32 cpDispSize;
    /* 0x1E8 */ u32 cpDispStride;
    /* 0x1EC */ u32 cpDisp;
    /* 0x1F0 */ u32 cpTexSrc;
    /* 0x1F4 */ u32 cpTexSize;
    /* 0x1F8 */ u32 cpTexStride;
    /* 0x1FC */ u32 cpTex;
    /* 0x200 */ GXBool cpTexZ;
    /* 0x204 */ u32 genMode;
    /* 0x208 */ GXTexRegion TexRegions[8];
    /* 0x288 */ GXTexRegion TexRegionsCI[4];
    /* 0x2C8 */ u32 nextTexRgn;
    /* 0x2CC */ u32 nextTexRgnCI;
    /* 0x2D0 */ GXTlutRegion TlutRegions[20];
    /* 0x410 */ GXTexRegionCallback texRegionCallback;
    /* 0x414 */ GXTlutRegionCallback tlutRegionCallback;
    /* 0x418 */ u32 nrmType;
    /* 0x41C */ GXBool hasNrms;
    /* 0x41D */ GXBool hasBiNrms;
    /* 0x420 */ u32 projType;
    /* 0x424 */ f32 projMtx[6];
    /* 0x43C */ f32 vpLeft;
    /* 0x440 */ f32 vpTop;
    /* 0x444 */ f32 vpWd;
    /* 0x448 */ f32 vpHt;
    /* 0x44C */ f32 vpNearz;
    /* 0x450 */ f32 vpFarz;
    /* 0x454 */ u8 fgRange;
    /* 0x458 */ f32 fgSideX;
    /* 0x45C */ u32 tImage0[8];
    /* 0x47C */ u32 tMode0[8];
    /* 0x49C */ u32 texmapId[16];
    /* 0x4DC */ u32 tcsManEnab;
    /* 0x4E0 */ u32 tevTcEnab;
    /* 0x4E4 */ u32 perf0;
    /* 0x4E8 */ u32 perf1;
    /* 0x4EC */ u32 perfSel;
    /* 0x4F0 */ GXBool inDispList;
    /* 0x4F1 */ GXBool dlSaveContext;
    /* 0x4F2 */ u8 dirtyVAT;
    /* 0x4F4 */ u32 dirtyState;
} GXData;

extern GXData* gx;

// Hardware register blocks, set up by GXInit.
extern volatile u32* __piReg;
extern volatile u16* __cpReg;
extern volatile u16* __peReg;
extern volatile u16* __memReg;

// dirtyState bits, flushed by __GXSetDirtyState.
#define GX_DIRTY_SU_TEX 0x1
#define GX_DIRTY_BP_MASK 0x2
#define GX_DIRTY_GEN_MODE 0x4
#define GX_DIRTY_VCD 0x8
#define GX_DIRTY_VAT 0x10

// The write-gather pipe as an object at a fixed address (MWCC `: addr`), as in
// the SDK. Unlike a pointer dereference, the compiler knows it cannot alias
// other data, so values like `gx` stay in registers across pipe writes.
volatile PPCWGPipe __GXWGFifo : 0xCC008000;

#define GX_WRITE_U8(ub) (__GXWGFifo.u8 = (u8)(ub))
#define GX_WRITE_U16(us) (__GXWGFifo.u16 = (u16)(us))
#define GX_WRITE_U32(ui) (__GXWGFifo.u32 = (u32)(ui))
#define GX_WRITE_F32(f) (__GXWGFifo.f32 = (f32)(f))

// BP (raster) register write.
#define GX_WRITE_RAS_REG(value) \
    do {                        \
        GX_WRITE_U8(0x61);      \
        GX_WRITE_U32(value);    \
    } while (0)

// CP register write.
#define GX_WRITE_CP_STRM_REG(addr, vtxfmt, data) \
    do {                                         \
        GX_WRITE_U8(0x08);                       \
        GX_WRITE_U8((addr) | (vtxfmt));          \
        GX_WRITE_U32(data);                      \
    } while (0)

// XF register write.
#define GX_WRITE_XF_REG(addr, data)       \
    do {                                  \
        GX_WRITE_U8(0x10);                \
        GX_WRITE_U32(0x1000 + (addr));    \
        GX_WRITE_U32(data);               \
    } while (0)

#define GX_WRITE_XF_REG_F(addr, data)     \
    do {                                  \
        GX_WRITE_U8(0x10);                \
        GX_WRITE_U32(0x1000 + (addr));    \
        GX_WRITE_F32(data);               \
    } while (0)

#define SET_REG_FIELD(reg, size, shift, val) \
    ((reg) = ((u32)(reg) & ~(((1 << (size)) - 1) << (shift))) | ((u32)(val) << (shift)))

#define GET_REG_FIELD(reg, size, shift) (((u32)(reg) >> (shift)) & ((1 << (size)) - 1))

// Public GX functions not (yet) in gx.h. Enums are passed as u32, GXBool as u8.
void GXFlush(void);
void GXSetCopyClear(GXColor clear_clr, u32 clear_z);
void GXInvalidateVtxCache(void);
void GXSetLineWidth(u8 width, u32 texOffsets);
void GXSetPointSize(u8 pointSize, u32 texOffsets);
void GXEnableTexOffsets(u32 coord, u8 line_enable, u8 point_enable);
void GXSetCoPlanar(u8 enable);
void GXSetClipMode(u32 mode);
void GXSetScissorBoxOffset(s32 x_off, s32 y_off);
void GXInvalidateTexAll(void);
GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback f);
GXTlutRegionCallback GXSetTlutRegionCallback(GXTlutRegionCallback f);
void GXInitTexCacheRegion(GXTexRegion* region, u8 is_32b_mipmap, u32 tmem_even, u32 size_even, u32 tmem_odd,
                          u32 size_odd);
void GXInitTlutRegion(GXTlutRegion* region, u32 tmem_addr, u32 tlut_size);
void GXSetTevKAlphaSel(u32 stage, u32 sel);
void GXSetZTexture(u32 op, u32 fmt, u32 bias);
void GXSetFogRangeAdj(u8 enable, u16 center, void* table);
void GXSetColorUpdate(u8 update_enable);
void GXSetAlphaUpdate(u8 update_enable);
void GXSetDither(u8 dither);
void GXSetDstAlpha(u8 enable, u8 alpha);
void GXSetPixelFmt(u32 pix_fmt, u32 z_fmt);
void GXSetFieldMask(u8 odd_mask, u8 even_mask);
void GXSetFieldMode(u8 field_mode, u8 half_aspect_ratio);
void GXSetDispCopyFrame2Field(u32 mode);
void GXSetCopyClamp(u32 clamp);
void GXSetCopyFilter(u8 aa, const u8 sample_pattern[12][2], u8 vf, const u8 vfilter[7]);
void GXSetDispCopyGamma(u32 gamma);
void GXCopyDisp(void* dest, u8 clear);
void GXClearBoundingBox(void);
void GXPokeAlphaMode(u32 func, u8 threshold);
void GXPokeAlphaRead(u32 mode);
void GXPokeAlphaUpdate(u8 update_enable);
void GXPokeBlendMode(u32 type, u32 src_factor, u32 dst_factor, u32 op);
void GXPokeColorUpdate(u8 update_enable);
void GXPokeDstAlpha(u8 enable, u8 alpha);
void GXPokeDither(u8 dither);
void GXPokeZMode(u8 compare_enable, u32 func, u8 update_enable);
void GXSetGPMetric(u32 perf0, u32 perf1);
void GXClearGPMetric(void);

// GXGeometry.c
void __GXSetDirtyState(void);
void __GXSendFlushPrim(void);
void __GXSetGenMode(void);

// GXAttr.c
void __GXSetVCD(void);
void __GXSetVAT(void);

// GXBump.c
void __GXUpdateBPMask(void);
void __GXFlushTextureState(void);

// GXTexture.c
void __GXSetSUTexRegs(void);
void __GXSetTmemConfig(u32 config);

// GXFifo.c
void __GXFifoInit(void);
void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size);
void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr);
void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark);
void GXSetCPUFifo(GXFifoObj* fifo);
void GXSetGPFifo(GXFifoObj* fifo);

// GXMisc.c
void __GXPEInit(void);

// GXStubs.c
void __GXSetRange(f32 nearz, f32 fgSideX);

// GXTransform.c
void __GXSetMatrixIndex(u32 matIdxAttr);

#ifdef __cplusplus
}
#endif

#endif
