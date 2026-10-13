// Hand-written: the triangle volume queries test against.
#ifndef _GAME_TRIANGLE_H_
#define _GAME_TRIANGLE_H_

#include "types.h"
#include "game/vector.h"

struct CTriangle {
    /* 0x00 */ CVector3 m_v0;
    /* 0x10 */ CVector3 m_v1;
    /* 0x20 */ CVector3 m_v2;
};

#endif
