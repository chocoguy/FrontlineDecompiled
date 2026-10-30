// CScreen: a render target / display. Partly hand-maintained (render mode fields typed).
#ifndef _GAME_SCREEN_H_
#define _GAME_SCREEN_H_

#include "types.h"
#include <dolphin/gx.h>
#include "game_types.h"
#include "game/color.h"
#include "game/matrix.h"

struct CColor;
class CMatrix;
// A pixel rectangle (size 0x10).
struct CRect {
    /* 0x0 */ s32 left;
    /* 0x4 */ s32 top;
    /* 0x8 */ s32 right;
    /* 0xC */ s32 bottom;
};
class CScreen;
class CScreenFieldMode;

// The display: double-buffered external frame buffers swapped at vertical
// retrace, the clear colour/rect, and the view-to-screen transform.
// size 0x110 (CMatrix alignment); vtable 0x80181DC4
class CScreen {
public:
    CScreen(EScreenWidth width, EFrameFormat frameFormat, EDepthFormat depthFormat) {
        m_clear = true;
        m_clearRect.left = 0;
        m_clearRect.top = 0;
        m_clearRect.right = 0;
        m_clearRect.bottom = 0;
        m_clearColor.r = 0;
        m_clearColor.g = 0;
        m_clearColor.b = 0;
        m_clearColor.a = 0x80;
        field_0x1C = 0;
        m_width = width;
        m_height = 448;
        m_frameFormat = frameFormat;
        m_depthFormat = depthFormat;
        field_0x54 = 0;
        field_0x58 = 0;
        m_viewToScreenValid = false;
        m_field = 0;
        m_retraces = 0;
        m_missedFrame = false;
    }
    virtual ~CScreen();  // 0x8001BEA4
    virtual CScreen* SetCurrent();  // 0x8001BB3C
    virtual int Flip();  // 0x8001BA5C
    virtual int Wait();  // 0x8001B910
    virtual void SetClear(bool);  // 0x8001B900
    virtual void SetClearColor(const CColor&);  // 0x8001B8DC
    virtual void SetClearRect(const CRect&);  // 0x8001B8B8
    virtual int GetHeight();  // 0x8001B8B0
    virtual int GetWidth();  // 0x8001B8A8
    virtual float GetHeightScale() { return m_heightScale; }  // 0x8001B12C
    virtual float GetWidthScale() { return m_widthScale; }  // 0x8001B134
    virtual const s32& GetFrameFormat();  // 0x8001B8A0
    virtual const s32& GetDepthFormat();  // 0x8001B898
    virtual bool GetClear();  // 0x8001B824
    virtual const CColor& GetClearColor();  // 0x8001B81C
    virtual const CRect& GetClearRect();  // 0x8001B814
    virtual const CMatrix& GetTMViewToScreen();  // 0x8001B690
    virtual void Rebuild() = 0;
    virtual void PutDisp();  // 0x8001B7E8
    virtual void Clear();  // 0x8001B7EC
    virtual void SetDraw() = 0;
    virtual void _Capture();  // 0x8001B658

    /* 0x04 */ u8 unk_0x4;
    /* 0x05 */ bool m_clear;
    /* 0x06 */ u8 unk_0x6[0x2];
    /* 0x08 */ CRect m_clearRect;
    /* 0x18 */ CColor m_clearColor;
    /* 0x1C */ u32 field_0x1C;
    /* 0x20 */ s32 m_width;  // EScreenWidth (640)
    /* 0x24 */ s32 m_height;
    /* 0x28 */ f32 m_widthScale;  // m_width / 640
    /* 0x2C */ f32 m_heightScale;  // m_height / 448
    /* 0x30 */ s32 m_frameFormat;  // EFrameFormat: 0 = 16-bit, 1 = 24-bit, else 32-bit
    /* 0x34 */ s32 m_depthFormat;  // EDepthFormat: 0 = 16-bit, 1 = 24-bit, else 28-bit
    /* 0x38 */ u8 unk_0x38[0x1C];
    /* 0x54 */ u32 field_0x54;
    /* 0x58 */ u32 field_0x58;
    /* 0x60 */ CMatrix m_viewToScreen;
    /* 0xA0 */ bool m_viewToScreenValid;
    /* 0xA4 */ u32 m_field;  // flips at each swap (interlace field)
    /* 0xA8 */ s32 m_retraces;  // retraces since the last swap
    /* 0xAC */ bool m_missedFrame;
    /* 0xB0 */ GXRenderModeObj* m_pRenderMode;  // points at m_renderMode
    /* 0xB4 */ GXRenderModeObj m_renderMode;
    /* 0xF0 */ void* m_frameBuffer1;
    /* 0xF4 */ void* m_frameBuffer2;
    /* 0xF8 */ void* m_pDrawBuffer;  // the frame buffer being drawn
    /* 0xFC */ void* m_fifo;
    /* 0x100 */ void* m_pFifoObj;

    static void Capture();  // 0x8001B684
    int GetDepthBufferMax();  // 0x8001B840
    static int GetVSyncs();  // 0x8001B908
    static CScreen* GetCurrent();  // 0x8001BE68
    static void Init();  // 0x8001BE70
    static void VBlankHandler(u32);  // 0x8001BF1C
#ifdef FRONTLINE_NATIVE
    // Widescreen (Rewritten): renders `width` of the console's pixels wide
    // from now on, with the 2D's 640 x 448 screen in the middle. For a
    // level; the screen stays that wide until the process restarts.
    void NativeSetRenderWidth(int width);
#endif
};

// The screen as interlaced fields (size 0x110); vtable 0x80181D64
class CScreenFieldMode : public CScreen {
public:
    virtual void _Capture();  // 0x8001B13C
    virtual void PutDisp();  // 0x8001B4B0
    virtual void SetDraw();  // 0x8001B4B4
    virtual void Rebuild();  // 0x8001B4B8
    virtual ~CScreenFieldMode();  // 0x8001B4BC
    virtual CScreen* SetCurrent();  // 0x8001B544
    CScreenFieldMode(EScreenWidth, EFrameFormat, EDepthFormat);  // 0x8001B5A0
};

extern CScreenFieldMode g_screen;  // 0x80196AC0 (Moh2.cpp)

#ifdef FRONTLINE_NATIVE
// Widescreen (CScreen::NativeSetRenderWidth): the render's width over the
// 2D screen's 640, and the 2D screen's margin on each side of it in the
// console's pixels; 1 and 0 at 4:3. The 3D view widens by the scale (same
// height, more to the sides) and 2D draws reach into the margins.
extern float g_nativeWideScale;
extern float g_nativeWideMargin;
// Added to the x of HUD sprites and text while set (the HUD's corners move
// out to the screen's; UserInterface::Draw).
extern float g_nativeHudShift;
#endif

#endif
