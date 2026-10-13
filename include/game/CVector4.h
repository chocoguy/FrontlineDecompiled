// CVector4: four-component float vector (homogeneous transform results).
// Hand-maintained.
#ifndef _GAME_CVECTOR4_H_
#define _GAME_CVECTOR4_H_

#include "types.h"

class CVector4 {
public:
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
    CVector4() {}  // 0x8006567C
};

#endif
