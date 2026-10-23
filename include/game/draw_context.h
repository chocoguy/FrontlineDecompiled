// CDrawContext: the matrices and state for drawing one view.
// Hand-maintained.
#ifndef _GAME_DRAW_CONTEXT_H_
#define _GAME_DRAW_CONTEXT_H_

#include "types.h"
#include "game/matrix.h"

class CCharacterShadow;
class CLight;
struct BPDLightVolume;
class ISceneNode;

// Character shadows queued for drawing this frame.
struct CShadowDrawList {
    /* 0x0 */ CCharacterShadow** items;
    /* 0x4 */ u32 field_0x4;
    /* 0x8 */ int count;
};

// size 0x118 (rounded up to 0x120 by CMatrix's 16-byte alignment; the game only
// creates these on the stack)
// Dynamic lights near the view (same shape as CShadowDrawList).
struct CLightDrawList {
    /* 0x0 */ CLight** items;
    /* 0x4 */ u32 field_0x4;
    /* 0x8 */ int count;
};

// m_2dReject: frustum planes the object reaches outside of. The writers
// disagree: CVolBox::TestVisibility ORs in small codes (right 1, left 2,
// top 4, bottom 8, far 0x10, near 0x20), CVolSphere::TestVisibility sets
// these top-byte bits.
enum {
    DRAW_REJECT_NEAR = 0x04000000,
    DRAW_REJECT_FAR = 0x08000000,
    DRAW_REJECT_BOTTOM = 0x10000000,
    DRAW_REJECT_TOP = 0x20000000,
    DRAW_REJECT_LEFT = 0x40000000,
    DRAW_REJECT_RIGHT = 0x80000000,
};

class CDrawContext {
public:
    /* 0x000 */ CMatrix m_ViewProj;   // *m_pView x *m_pProj
    /* 0x040 */ CMatrix m_ClipScreen;  // *m_pClip x *m_pScreen
    /* 0x080 */ CMatrix m_ViewScreen;  // m_ViewProj x m_ClipScreen
    /* 0x0C0 */ const CMatrix* m_pWorld;  // object being drawn (translation in row 3)
    /* 0x0C4 */ const CMatrix* m_pView;
    /* 0x0C8 */ const CMatrix* m_pProj;
    /* 0x0CC */ const CMatrix* m_pClip;
    /* 0x0D0 */ const CMatrix* m_pScreen;
    /* 0x0D4 */ ISceneNode* m_pCamera;
    /* 0x0D8 */ uptr field_0xD8;  // CFog* (pointer-sized natively)
    /* 0x0DC */ u8 unk_0xDC[0x4];
    /* 0x0E0 */ f32 m_scale[3];
    /* 0x0EC */ u8 unk_0xEC[0x4];
    /* 0x0F0 */ CLightDrawList* m_pLights;
    /* 0x0F4 */ CShadowDrawList* m_pShadows;
    /* 0x0F8 */ BPDLightVolume* m_pLightVolume;  // of the object being drawn
    /* 0x0FC */ CLight* m_pNearestLight;          // dynamic light used for it, if any
    /* 0x100 */ u32 m_2dReject;  // DRAW_REJECT_* planes the object crosses
    /* 0x104 */ f32 m_screenMin[2];  // screen rect of the last TestVisibility
    /* 0x10C */ f32 m_screenMax[2];
    /* 0x114 */ int field_0x114;

    CDrawContext(const CMatrix& view, const CMatrix& proj, const CMatrix& clip,
                 const CMatrix& screen, int arg4);  // 0x8007761C
};

#endif
