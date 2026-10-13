// CFont: text drawn with EA's FONT library through a driver that queues
// glyph quads on the render list. Hand-maintained.
#ifndef _GAME_FONT_H_
#define _GAME_FONT_H_

#include "types.h"
#include "game_types.h"
#include "game/color.h"
#include "game/renderbin.h"
#include "game/texture.h"
#include "ea/sys/font.h"

class CFont;
class CRenderList;
class CScreen;

// The FONT driver: callbacks the library makes while drawing a string.
class CFontDriver : public FONTDRIVER {
public:
    CFontDriver() {
        draw = Draw;
        startDraw = StartDraw;
        endDraw = EndDraw;
        createFont = CreateFont;
        destroyFont = DestroyFont;
    }
    static void DestroyFont(FONT*);  // 0x8007C0FC
    static void CreateFont(FONT*);  // 0x8007C100
    static void EndDraw(FONT*);  // 0x8007C194
    static void StartDraw(FONT*);  // 0x8007C200
    static void Draw(FONT*, const FONTCHAR*, float, float);  // 0x8007C210
    ~CFontDriver();  // 0x8007C2E4
};

// A font's texture, created by CFontDriver::CreateFont (size 0x48).
struct CFontTexture : public CTexture {
    /* 0x40 */ CFont* m_owner;
    /* 0x44 */ FONT* m_font;
};

// size 0x28 (operator new); vtable 0x801847A8
class CFont : public CRenderBin {
public:
    /* 0x20 */ FONT* m_font;
    /* 0x24 */ CFontTexture* m_texture;

    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x8007C320
    virtual int Render(CDmaPacket&, void*);  // 0x8007CA34
    float GetHeight();  // 0x8007CB04
    void SetHeight(float);  // 0x8007CB10
    void SetScale(float, float);  // 0x8007CB1C
    void GetScale(float&, float&);  // 0x8007CB30
    void SetDepth(unsigned long);  // 0x8007CB4C
    CColor GetColor();  // 0x8007CB68
    void SetColor(CColor);  // 0x8007CBA0
    void DrawTextCenteredF(float y, const char* fmt, ...);  // 0x8007CBB0
    void DrawTextCentered(float y, const char*);  // 0x8007CDB0
    void DrawText(float x, float y, const char*);  // 0x8007CF50
    static void InitClass(CRenderList*, CScreen*);  // 0x8007D004
    virtual ~CFont();  // 0x8007D0C0
    CFont(const char*, ERenderPriority);  // 0x8007D148
};

#endif
