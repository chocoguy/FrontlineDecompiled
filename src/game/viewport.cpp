#include "game/viewport.h"

#include <dolphin/gx.h>
#include <dolphin/vi.h>
#include "game/Screen.h"

void CViewport::SetRect(float left, float top, float right, float bottom) {
    m_left = left;
    m_top = top;
    m_bottom = bottom;
    m_right = right;
    m_width = right - left;
    m_height = bottom - top;
    m_clipToViewValid = false;
}

void CViewport::SetViewport(CScreen& screen) const {
    int screenWidth = screen.GetWidth();
    int screenHeight = screen.GetHeight();
    float x = screenWidth * m_left;
    float y = screenHeight * m_top;
    float w = screenWidth * m_width;
    float h = screenHeight * m_height;
    if (screen.m_pRenderMode->field_rendering) {
        GXSetViewportJitter(x, y, w, h, 0.0f, 1.0f, VIGetNextField());
    } else {
        GXSetViewport(x, y, w, h, 0.0f, 1.0f);
    }
    GXSetScissor(x, y, w, h);
}

// Maps clip space onto this viewport's rectangle (in clip space of the screen).
const CMatrix& CViewport::GetTMClipToView() const {
    if (!m_clipToViewValid) {
        CVector3 scale(m_right - m_left, m_bottom - m_top, 1.0f);
        CVector3 pos((m_right + m_left) - 1.0f, -((m_bottom + m_top) - 1.0f), 0.0f);
        CViewport* self = const_cast<CViewport*>(this);
        self->m_clipToView.BuildScale(scale);
        self->m_clipToView.SetPos(pos);
        self->m_clipToViewValid = true;
    }
    return m_clipToView;
}
