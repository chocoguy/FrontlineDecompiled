// GCSW_VD: the movie player's software video driver. It writes a decoded
// frame straight into the external frame buffer as YUYV, each row twice.
// Hand-maintained.
#ifndef _MOVIE_SWVIDEODISPLAYLIST_H_
#define _MOVIE_SWVIDEODISPLAYLIST_H_

#include "types.h"
#include "game_types.h"
#include "dolphin/gx.h"
#include "movie/rcmp.h"

// size 0x1C; vtable 0x8018C47C
class GCSW_VD : public RCMP::VIDEO_DRIVER {
public:
    /* 0x04 */ RCMP::DECODER* m_decoder;
    /* 0x08 */ RCMP::FRAME* m_frame;  // released by CompleteDraw
    /* 0x0C */ GXRenderModeObj* m_renderMode;
    /* 0x10 */ void* m_xfb;
    /* 0x14 */ int m_x;
    /* 0x18 */ int m_y;

    RCMP_ALLOCATOR
    GCSW_VD(RCMP::DECODER* decoder, RCMP::FRAME* frame, GXRenderModeObj* rmode, void* xfb);
    virtual ~GCSW_VD();  // 0x80108648
    virtual void SetPosition(float x, float y, float z);  // 0x80108210
    virtual void SetScale(float x, float y);  // 0x8010820C
    virtual void Draw(RCMP::FRAME* frame);  // 0x801081DC
    virtual void ReDraw();  // 0x80108104
    virtual void CompleteDraw();  // 0x801080C8
};

#endif
