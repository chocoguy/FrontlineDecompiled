#include "game/draw_context.h"

#include <dolphin/gx.h>

CDrawContext::CDrawContext(const CMatrix& view, const CMatrix& proj, const CMatrix& clip,
                           const CMatrix& screen, int arg4) {
    m_pWorld = NULL;
    m_pView = &view;
    m_pProj = &proj;
    m_pClip = &clip;
    m_pScreen = &screen;
    m_pCamera = NULL;
    field_0xD8 = 0;
    m_scale[0] = 1.0f;
    m_scale[1] = 1.0f;
    m_scale[2] = 1.0f;
    m_pLights = NULL;
    m_pLightVolume = NULL;
    m_pNearestLight = NULL;
    field_0x114 = arg4;

    m_ViewProj.Multiply(*m_pView, *m_pProj);
    m_ClipScreen.Multiply(*m_pClip, *m_pScreen);
    m_ViewScreen.Multiply(m_ViewProj, m_ClipScreen);

    GXSetProjection(m_pProj->m, GX_PERSPECTIVE);
    f32 mtx[3][4];
    m_pView->GetGAMECUBEMatrix34(mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX0);
}
