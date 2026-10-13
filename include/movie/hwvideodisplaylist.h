// GCHW_VD: the movie player's GX video driver. It draws a decoded frame's
// Y, U and V planes as C8 textures through tBigYUVSwizzler, centred on the
// screen. Hand-maintained.
#ifndef _MOVIE_HWVIDEODISPLAYLIST_H_
#define _MOVIE_HWVIDEODISPLAYLIST_H_

#include "types.h"
#include "game_types.h"
#include "dolphin/gx.h"
#include "movie/rcmp.h"
#include "movie/bigyuvswizzler.h"

// size 0xA0; vtable 0x8018C2E4
class GCHW_VD : public RCMP::VIDEO_DRIVER {
public:
    /* 0x04 */ RCMP::DECODER* m_decoder;
    /* 0x08 */ tBigYUVSwizzler* m_swizzler;
    /* 0x0C */ GXTexObj m_yTex;  // tlut 2
    /* 0x2C */ GXTexObj m_uTex;  // tlut 0
    /* 0x4C */ GXTexObj m_vTex;  // tlut 1
    /* 0x6C */ RCMP::FRAME* m_frame;  // held until the next one
    /* 0x70 */ f32 m_width;
    /* 0x74 */ f32 m_height;
    /* 0x78 */ f32 m_z;
    /* 0x7C */ f32 m_x0;
    /* 0x80 */ f32 m_y0;
    /* 0x84 */ f32 m_x1;
    /* 0x88 */ f32 m_y1;
    /* 0x8C */ f32 m_scaleX;
    /* 0x90 */ f32 m_scaleY;
    /* 0x94 */ u32 m_screenHeight;
    /* 0x98 */ u32 m_screenWidth;
    /* 0x9C */ GXRenderModeObj* m_renderMode;

    RCMP_ALLOCATOR
    GCHW_VD(RCMP::DECODER* decoder, RCMP::FRAME* frame);
    virtual ~GCHW_VD();  // 0x801028D4
    virtual void SetPosition(float x, float y, float z);  // 0x801029A0
    virtual void SetScale(float x, float y);  // 0x8010296C
    virtual void Draw(RCMP::FRAME* frame);  // 0x801022B4
    virtual void ReDraw();  // 0x80102294
    virtual void CompleteDraw();  // 0x80102290
    void iDraw();  // 0x80102308
};

#endif
