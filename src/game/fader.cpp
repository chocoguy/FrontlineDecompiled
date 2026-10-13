#include "game/fader.h"

#include <dolphin/gx.h>
#include "game/mathfun.h"
#include "game/renderlist.h"

static CFaderBin g_FaderBin;
static CFaderControl* g_pFaderControl;
static CRenderList* g_pRenderList;
static CColor g_color(0, 0, 0, 0);

void CFader::Init(CRenderList* renderList) {
    g_pRenderList = renderList;
    renderList->Register(g_FaderBin);
}

void CFader::Update(float dt) {
    if (g_pFaderControl != NULL) {
        g_pFaderControl->Update(dt);
        if (!g_pFaderControl->IsFading()) {
            g_pFaderControl = NULL;
        }
    }
}

void CFader::SetColor(CColor color) {
    g_color = color;
}

void CFader::SetControl(CFaderControl* control) {
    if (control != NULL) {
        control->m_time = 0.0f;
        g_color = CColor(control->m_from.r, control->m_from.g, control->m_from.b,
                         control->m_from.a);
    }
    g_pFaderControl = control;
}

CFaderControl::CFaderControl(CColor from, CColor to, float duration) {
    m_from = from;
    m_to = to;
    m_duration = duration;
    m_time = 0.0f;
}

// Still fading (including the last step).
bool CFaderControl::IsFading() const {
    return m_time <= m_duration;
}

static inline u8 Lerp(u8 from, u8 to, float t) {
    float step = ((float)to - (float)from) * t;
    return (u8)(int)(step + (float)from);
}

void CFaderControl::Update(float dt) {
    if (m_time < m_duration) {
        float t = m_time / m_duration;
        float time = m_time + dt;
        g_color = CColor(Lerp(m_from.r, m_to.r, t), Lerp(m_from.g, m_to.g, t),
                         Lerp(m_from.b, m_to.b, t), Lerp(m_from.a, m_to.a, t));
        m_time = time;
        if (m_time > m_duration) {
            m_time = m_duration;
        }
    } else if (m_time == m_duration) {
        g_color = m_to;
        m_time = m_time + dt;
    }
}

// Colors are doubled (the vertex color range is 0..128 for full intensity).
static inline void FaderVertex(s16 x, s16 y) {
    GXPosition3s16(x, y, 0);
    GXColor4u8(MathFunClamp<int>(g_color.r * 2, 0, 255), MathFunClamp<int>(g_color.g * 2, 0, 255),
               MathFunClamp<int>(g_color.b * 2, 0, 255), MathFunClamp<int>(g_color.a * 2, 0, 255));
}

// Draws a screen-covering quad in g_color.
void CFaderBin::Link(CDmaTag*, CDmaPacket&) {
    f32 savedProj[7];
    GXGetProjectionv(savedProj);
    f32 proj[4][4];
    C_MTXOrtho(proj, 224.0f, -224.0f, -320.0f, 320.0f, 5.0f, 2000.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    f32 view[3][4];
    PSMTXIdentity(view);
    view[2][3] = -1200.0f;
    GXLoadPosMtxImm(view, GX_PNMTX0);

    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);   // GX_VA_POS, GX_DIRECT
    GXSetVtxDesc(11, 1);  // GX_VA_CLR0, GX_DIRECT
    GXSetVtxDesc(13, 0);  // GX_VA_TEX0, GX_NONE
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);   // xyz s16
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);  // rgba8
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);  // GX_PASSCLR
    GXSetBlendMode(1, 4, 5, 5);
    GXSetZMode(0, 7, 0);
    GXSetCullMode(0);

    GXBegin(0x80, 0, 4);  // GX_QUADS
    FaderVertex(-320, 224);
    FaderVertex(320, 224);
    FaderVertex(320, -224);
    FaderVertex(-320, -224);
    GXEnd();

    GXSetProjectionv(savedProj);
}

bool CFaderBin::IsUsed() {
    return g_color.a != 0;
}
