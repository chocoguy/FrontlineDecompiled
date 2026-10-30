// 2D sprites: screen-space images and polygons, queued through the render
// list and drawn with GX in immediate mode.
#include "game/sprite.h"
#include <dolphin/gx.h>
#include "game/Moh2.h"
#include "game/Screen.h"
#include "game/gameconfig.h"
#include "game/mathfun.h"
#include "game/renderlist.h"
#include "game/shape.h"
#include "game/toilet.h"
#include "game/viewport.h"

static CSpriteNoTextureBin g_SpriteNoTexture;
static CSpriteDrawContext g_DrawContext;
static float g_HeightScale = 1.0f;
static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static float g_ScreenXOffset;
static float g_ScreenYOffset;

void CSpriteNoTextureBin::Link(CDmaTag*, CDmaPacket&) {}

int CSpriteNoTextureBin::Render(CDmaPacket&, void*) {
    return 0;
}

CSprite::CSprite(const char* name, CSprite::ESpriteLoadType type, ERenderPriority priority) {
    int size;
    m_type = priority;
    m_file = NULL;
    if (name == NULL) {
        return;
    }

    ShapeFile* file;
    switch (type) {
    case LOAD_BRIEFING_LOG:
        file = (ShapeFile*)TLT_LoadFileFromShellBriefingLogBigFile(name, &size);
        break;
    case LOAD_SHELL:
        file = (ShapeFile*)TLT_LoadFileFromShellBigFile(name, &size);
        break;
    case LOAD_SLIDES:
        file = (ShapeFile*)TLT_LoadFileFromSlidesBigFile(name, &size);
        break;
    default:
        file = (ShapeFile*)TLT_LoadFileFromLevelBigFile(name, &size);
        break;
    }
    if (file == NULL) {
        DebugMsg("CSprite::CSprite(%s):  TLT_LoadFile... FAILED!!!\n", name);
        return;
    }
#ifdef FRONTLINE_NATIVE
    // "SHPN": already swapped to little-endian (see ShapeFileFromBigEndian).
    if (file->magic[0] != 'S' || file->magic[1] != 'H' || file->magic[2] != 'P' ||
        (file->magic[3] != 'G' && file->magic[3] != 'N')) {
#else
    if (file->magic[0] != 'S' || file->magic[1] != 'H' || file->magic[2] != 'P' ||
        file->magic[3] != 'G') {
#endif
        DebugMsg("CCSprite::CCSprite():  File is not a GAMECUBE Shape, type = %s\n", file);
        return;
    }
    m_file = file;
    g_pRenderList->Register(*this);
    m_texture.Set(m_file, false);
}

#ifdef FRONTLINE_NATIVE
CSprite::CSprite(ShapeFile* file, ERenderPriority priority) {
    m_type = priority;
    m_file = file;
    g_pRenderList->Register(*this);
    m_texture.Set(m_file, false);
}
#endif

CSprite::~CSprite() {
    g_pRenderList->UnRegister(*this);
    if (m_file != NULL) {
        TLT_CloseFile(m_file);
        m_file = NULL;
    }
}

void CSprite::InitClass(CRenderList* renderList, CScreen* screen) {
    g_pRenderList = renderList;
    renderList->Register(g_SpriteNoTexture);
    g_pScreen = screen;
    g_ScreenXOffset = 2048.0f - (screen->GetWidth() >> 1);
    float halfHeight = g_pScreen->GetHeight() >> 1;
    g_HeightScale = 1.4285715f;
    g_HeightScale = 1.4285715f * 0.5f;
    g_ScreenYOffset = 2048.0f - halfHeight;
}

void CSprite::Draw(float x, float y, CColor color) {
    if (g_config.noSprites) {
        return;
    }
    g_DrawContext.sprite = this;
    g_DrawContext.type = 0;
    g_DrawContext.x = x;
    g_DrawContext.y = y;
    g_DrawContext.color.r = color.r;
    g_DrawContext.color.g = color.g;
    g_DrawContext.color.b = color.b;
    g_DrawContext.color.a = color.a;
    g_pRenderList->Render(*this, &g_DrawContext);
}

void CSprite::DrawPoly(_spritepolyvert* verts, int numVerts, CSprite::EPolyPrimType prim) {
    if (g_config.noSprites) {
        return;
    }
    g_DrawContext.verts = verts;
    g_DrawContext.numVerts = numVerts;
    g_DrawContext.sprite = this;
    g_DrawContext.type = 1;
    g_DrawContext.prim = prim;
    g_pRenderList->Render(*this, &g_DrawContext);
}

void CSprite::DrawPolyNoTex(_spritepolyvert* verts, int numVerts, CSprite::EPolyPrimType prim) {
    if (g_config.noSprites) {
        return;
    }
    g_DrawContext.verts = verts;
    g_DrawContext.numVerts = numVerts;
    g_DrawContext.prim = prim;
    g_DrawContext.sprite = NULL;
    g_DrawContext.type = 1;
    g_pRenderList->Render(g_SpriteNoTexture, &g_DrawContext);
}

// Copies the draw (and its vertices) into the packet.
int CSprite::Render(CDmaPacket& packet, void* data) {
    CSpriteDrawContext* ctx = (CSpriteDrawContext*)data;
    switch (ctx->type) {
    case 0:
        *(CSpriteDrawContext*)packet.m_ptr = *ctx;
#ifdef FRONTLINE_NATIVE
        ((CSpriteDrawContext*)packet.m_ptr)->x += g_nativeHudShift;
#endif
        packet.m_ptr += sizeof(CSpriteDrawContext);
        break;
    case 1: {
        int n = ctx->numVerts;
        _spritepolyvert* v = ctx->verts;
        *(CSpriteDrawContext*)packet.m_ptr = *ctx;
        packet.m_ptr += sizeof(CSpriteDrawContext);
        for (int i = 0; i < n; i++) {
            *(_spritepolyvert*)packet.m_ptr = v[i];
#ifdef FRONTLINE_NATIVE
            ((_spritepolyvert*)packet.m_ptr)->x += g_nativeHudShift;
#endif
            packet.m_ptr += sizeof(_spritepolyvert);
        }
        break;
    }
    }
    return 0;
}

// Vertex colors are 0..0x7F (0x80 = full); GX wants 0..0xFF.
static inline u8 VtxColor(u8 c) {
    return MathFunClamp<int>(c * 2, 0, 0xFF);
}

void CSprite::Link(CDmaTag* tags, CDmaPacket& packet) {
    if (tags == NULL) {
        return;
    }

    f32 savedProjection[7];
    f32 projection[4][4];
    f32 view[3][4];
    GXGetProjectionv(savedProjection);
#ifdef FRONTLINE_NATIVE
    // Widescreen: the 640 x 448 screen in the middle, the margins beyond it.
    C_MTXOrtho(projection, 448.0f * g_viewport.m_top, 448.0f * g_viewport.m_bottom,
               (640.0f + 2 * g_nativeWideMargin) * g_viewport.m_left - g_nativeWideMargin,
               (640.0f + 2 * g_nativeWideMargin) * g_viewport.m_right - g_nativeWideMargin, 5.0f, 2000.0f);
#else
    C_MTXOrtho(projection, 448.0f * g_viewport.m_top, 448.0f * g_viewport.m_bottom,
               640.0f * g_viewport.m_left, 640.0f * g_viewport.m_right, 5.0f, 2000.0f);
#endif
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    PSMTXIdentity(view);
    view[2][3] = -1200.0f;
    GXLoadPosMtxImm(view, 0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);    // position, direct
    GXSetVtxDesc(0xB, 1);  // color 0
    GXSetVtxDesc(0xD, 1);  // tex coord 0
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);    // xyz s16
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);  // rgba8
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);  // st f32
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTevStages(1);
    GXSetBlendMode(1, 4, 5, 5);
    GXSetZMode(0, 7, 0);
    GXSetCullMode(0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetAlphaCompare(7, 0, 0, 7, 0);

    for (CDmaTag* tag = tags; tag != NULL; tag = tag->m_next) {
        CSpriteDrawContext* ctx = (CSpriteDrawContext*)tag->m_data;
        m_texture.Load(packet, NULL);
        if (ctx->type == 0) {
            // The image at its size, top-left at (x, y).
            float x0 = ctx->x;
            float y0 = ctx->y;
            float x1 = (float)m_texture.m_width * g_screen.m_widthScale + x0;
            u8 r = ctx->color.r;
            u8 g = ctx->color.g;
            u8 b = ctx->color.b;
            u8 a = ctx->color.a;
            float y1 = (float)(u16)(m_texture.m_height * g_HeightScale) * g_screen.m_heightScale + y0;
            GXBegin(0x80, 0, 4);  // quads
            GXPosition3s16(x0, y0, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3s16(x1, y0, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3s16(x1, y1, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3s16(x0, y1, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(0.0f, 1.0f);
        } else if (ctx->type == 1) {
            int n = ctx->numVerts;
            if (ctx->prim == POLY_TRIANGLE_FAN) {
                GXBegin(0xA0, 0, n);
            } else if (ctx->prim == POLY_TRIANGLE_STRIP) {
                GXBegin(0x90, 0, n);
            }
            _spritepolyvert* verts = (_spritepolyvert*)(ctx + 1);
            for (int i = 0; i < n; i++) {
                _spritepolyvert v = verts[i];
                GXPosition3s16(v.x, v.y, 10);
                GXColor4u8(VtxColor(v.color[0]), VtxColor(v.color[1]), VtxColor(v.color[2]),
                           VtxColor(v.color[3]));
                GXTexCoord2f32(v.u, v.v);
            }
        }
    }
    GXSetProjectionv(savedProjection);
}

void CSprite::Unload() {
    if (m_file != NULL) {
        TLT_CloseFile(m_file);
        m_file = NULL;
    }
}
