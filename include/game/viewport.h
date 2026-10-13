// CViewport: a sub-rectangle of a screen in normalized (0..1) coordinates.
// Hand-maintained.
#ifndef _GAME_VIEWPORT_H_
#define _GAME_VIEWPORT_H_

#include "types.h"
#include "game/matrix.h"

class CScreen;

// size >= 0x64 (0x70 with CMatrix's alignment); embedded in the game app at 0x460
class CViewport {
public:
    /* 0x00 */ f32 m_left;
    /* 0x04 */ f32 m_top;
    /* 0x08 */ f32 m_right;
    /* 0x0C */ f32 m_bottom;
    /* 0x10 */ f32 m_width;
    /* 0x14 */ f32 m_height;
    /* 0x18 */ s32 m_index;
    // Cached clip-space to viewport transform, rebuilt when m_clipToViewValid is clear.
    /* 0x20 */ CMatrix m_clipToView;
    /* 0x60 */ bool m_clipToViewValid;

    CViewport(float left = 0.0f, float top = 0.0f, float right = 1.0f, float bottom = 1.0f, int index = 0) {
        m_left = left;
        m_top = top;
        m_right = right;
        m_bottom = bottom;
        m_width = right - left;
        m_height = bottom - top;
        m_index = index;
        m_clipToViewValid = false;
    }

    const CMatrix& GetTMClipToView() const;  // 0x8007914C
    void SetViewport(CScreen& screen) const;  // 0x80079210
    void SetRect(float left, float top, float right, float bottom);  // 0x800793B4
};

extern CViewport g_viewport;  // 0x80194CA0 (Moh2.cpp)
extern CViewport g_mpViewports[6];  // 0x80194D10 (Moh2.cpp; 2 split-screen halves, then 4 quarters)

#endif
