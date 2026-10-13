// CFont: text through EA's FONT library. FONT_drawtexta calls the driver,
// which collects the glyphs into stringPacket and queues it on the render
// list; Link then draws the glyphs as textured quads.
#include "game/font.h"
#include <dolphin/gx.h>
#include "game/Screen.h"
#include "game/mathfun.h"
#include "game/renderlist.h"
#include "game/shape.h"
#include "game/viewport.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

#define FONT_MAX_GLYPHS 256

// A glyph as queued (size 0x10).
struct CFontGlyph {
    /* 0x0 */ u8 width;
    /* 0x1 */ u8 height;
    /* 0x2 */ u16 texX;
    /* 0x4 */ u16 texY;
    /* 0x8 */ f32 x;
    /* 0xC */ f32 y;
};

// One string's draw (size 0x1010).
struct CFontPacket {
    /* 0x00 */ s32 numGlyphs;
    /* 0x04 */ f32 scaleX;
    /* 0x08 */ f32 scaleY;
    /* 0x0C */ CColor color;
    /* 0x10 */ CFontGlyph glyphs[FONT_MAX_GLYPHS];

    CFontPacket() : color(0, 0, 0, 0x80) {}
};

static CFontPacket stringPacket;
static CFontDriver g_FontDriver;
static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static bool g_bReady;
static CFont* g_pCurrentFont;  // the CFont being created (for CreateFont)
static float g_ScreenXOffset;
static float g_ScreenYOffset;

void CFontDriver::DestroyFont(FONT*) {}

// Builds the font's texture from the Shape inside the font file.
void CFontDriver::CreateFont(FONT* font) {
    if (g_pCurrentFont == NULL) {
        return;
    }
    Shape* shape = (Shape*)((u8*)font + font->shapeOffset);
    CFontTexture* texture = (CFontTexture*)operator new(sizeof(CFontTexture));
    if (texture != NULL) {
        memset(texture, 0, sizeof(CFontTexture));
    }
    texture->Set(shape, false);
    texture->m_font = font;
    g_pCurrentFont->m_texture = texture;
    texture->m_owner = g_pCurrentFont;
    font->user = g_pCurrentFont;
}

void CFontDriver::EndDraw(FONT* font) {
    CFont* owner = (CFont*)font->user;
    FONT* f = owner->m_font;
    CColor color = *(CColor*)&f->color;
    stringPacket.color.g = color.g;
    stringPacket.color.r = color.r;
    stringPacket.color.b = color.b;
    stringPacket.color.a = color.a;
    stringPacket.scaleX = f->scaleX;
    stringPacket.scaleY = f->scaleY;
    g_pRenderList->Render(*owner, &stringPacket);
}

void CFontDriver::StartDraw(FONT*) {
    stringPacket.numGlyphs = 0;
}

void CFontDriver::Draw(FONT*, const FONTCHAR* ch, float x, float y) {
    stringPacket.glyphs[stringPacket.numGlyphs].width = ch->width;
    stringPacket.glyphs[stringPacket.numGlyphs].height = ch->height;
    stringPacket.glyphs[stringPacket.numGlyphs].texX = ch->texX;
    stringPacket.glyphs[stringPacket.numGlyphs].texY = ch->texY;
    stringPacket.glyphs[stringPacket.numGlyphs].x = x + ch->xOffset;
    stringPacket.glyphs[stringPacket.numGlyphs].y = y + ch->yOffset;
    stringPacket.numGlyphs++;
}

CFontDriver::~CFontDriver() {}

// Vertex colors are 0..0x7F (0x80 = full); GX wants 0..0xFF.
static inline u8 VtxColor(u8 c) {
    return MathFunClamp<int>(c * 2, 0, 0xFF);
}

void CFont::Link(CDmaTag* tags, CDmaPacket& packet) {
    if (tags == NULL) {
        return;
    }

    f32 savedProjection[7];
    f32 projection[4][4];
    f32 view[3][4];
    GXGetProjectionv(savedProjection);
    C_MTXOrtho(projection, 448.0f * g_viewport.m_top, 448.0f * g_viewport.m_bottom,
               640.0f * g_viewport.m_left, 640.0f * g_viewport.m_right, 5.0f, 2000.0f);
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    PSMTXIdentity(view);
    view[2][3] = -1200.0f;
    GXLoadPosMtxImm(view, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTevStages(2);
    GXSetBlendMode(1, 4, 5, 5);
    GXSetZMode(0, 7, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetTevOrder(1, 0xFF, 0x100, 0xFF);
    GXSetTevColorIn(1, 0xF, 0xF, 0xF, 0);
    GXSetTevColorOp(1, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(1, 7, 7, 7, 0);
    GXSetTevAlphaOp(1, 0, 0, 1, 1, 0);
    m_texture->Load(packet, NULL);

    for (CDmaTag* tag = tags; tag != NULL; tag = tag->m_next) {
        CFontPacket* p = (CFontPacket*)tag->m_data;
        int n = p->numGlyphs;
        if (n == 0) {
            continue;
        }
        float sx = p->scaleX;
        float sy = p->scaleY;
        u8 r = p->color.r;
        u8 g = p->color.g;
        u8 b = p->color.b;
        u8 a = p->color.a;
        GXBegin(0x80, 0, n * 4);  // quads
        CFontGlyph* glyph = p->glyphs;
        for (int i = 0; i < n; i++, glyph++) {
            s16 texWidth = m_texture->m_width;
            s32 texHeight = m_texture->m_height;
            float x0 = sx * glyph->x;
            float y0 = sy * glyph->y;
            float u0 = (float)glyph->texX / (float)texWidth;
            float u1 = (float)(glyph->texX + glyph->width) / (float)texWidth;
            float v0 = (float)glyph->texY / (float)texHeight;
            float v1 = (float)(glyph->texY + glyph->height) / (float)texHeight;
            float x1 = sx * (glyph->x + glyph->width);
            float y1 = sy * (glyph->y + glyph->height);
            GXPosition3s16(x0, y0, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(u0, v0);
            GXPosition3s16(x1, y0, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(u1, v0);
            GXPosition3s16(x1, y1, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(u1, v1);
            GXPosition3s16(x0, y1, 0);
            GXColor4u8(VtxColor(r), VtxColor(g), VtxColor(b), VtxColor(a));
            GXTexCoord2f32(u0, v1);
        }
    }
    GXSetProjectionv(savedProjection);
}

// Copies the string's header and glyphs into the packet.
int CFont::Render(CDmaPacket& packet, void* data) {
    CFontPacket* p = (CFontPacket*)data;
    *(s32*)packet.m_ptr = p->numGlyphs;
    packet.m_ptr += 4;
    *(f32*)packet.m_ptr = p->scaleX;
    packet.m_ptr += 4;
    *(f32*)packet.m_ptr = p->scaleY;
    packet.m_ptr += 4;
    CColor* c = (CColor*)packet.m_ptr;
    c->r = p->color.r;
    c->g = p->color.g;
    c->b = p->color.b;
    c->a = p->color.a;
    packet.m_ptr += 4;
    u32 size = p->numGlyphs * sizeof(CFontGlyph);
    memcpy(packet.m_ptr, p->glyphs, size);
    packet.m_ptr += size;
    return 0;
}

float CFont::GetHeight() {
    return m_font->height;
}

void CFont::SetHeight(float height) {
    m_font->height = height;
}

void CFont::SetScale(float x, float y) {
    m_font->scaleX = x;
    m_font->scaleY = y;
}

void CFont::GetScale(float& x, float& y) {
    x = m_font->scaleX;
    y = m_font->scaleY;
}

void CFont::SetDepth(unsigned long depth) {
    m_font->depth = *(f32*)&depth;
}

CColor CFont::GetColor() {
    CColor color = *(CColor*)&m_font->color;
    return CColor(color.r, color.g, color.b, color.a);
}

void CFont::SetColor(CColor color) {
    m_font->color = *(u32*)&color;
}

// x for `text` to be centered in the viewport.
static inline float CenteredX(FONT* font, const char* text) {
    CScreen* screen = CScreen::GetCurrent();
    float width = screen->GetWidth() * g_viewport.m_width;
    float scale = screen->GetWidthScale();
    float center = (width * 0.5f + screen->GetWidth() * g_viewport.m_left) / scale;
    float rx, ry, w, h;
    FONT_getrecta(font, text, &rx, &ry, &w, &h);
    return -(w * 0.5f - center) * screen->GetWidthScale();
}

void CFont::DrawTextCenteredF(float y, const char* fmt, ...) {
    char text[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(text, fmt, args);
    va_end(args);
    DrawText(CenteredX(m_font, text), y, text);
}

void CFont::DrawTextCentered(float y, const char* text) {
    DrawText(CenteredX(m_font, text), y, text);
}

void CFont::DrawText(float x, float y, const char* text) {
    float sx = m_font->scaleX;
    float sy = m_font->scaleY;
    if (strlen(text) != 0) {
        m_font->user = this;
        FONT_drawtexta(m_font, text, x / sx, y / sy);
    }
}

void CFont::InitClass(CRenderList* renderList, CScreen* screen) {
    g_pRenderList = renderList;
    g_pScreen = screen;
    g_ScreenXOffset = 2048.0f - (screen->GetWidth() >> 1);
    g_ScreenYOffset = 2048.0f - (g_pScreen->GetHeight() >> 1);
    FONT_installdriver(&g_FontDriver);
    FONT_init();
    g_bReady = true;
}

CFont::~CFont() {
    g_pRenderList->UnRegister(*this);
    FONT_destroy(m_font);
    m_font = NULL;
}

CFont::CFont(const char* data, ERenderPriority priority) {
    m_type = priority;
    g_pCurrentFont = this;
    m_font = FONT_create(data);
    g_pCurrentFont = NULL;
    g_pRenderList->Register(*this);
}
