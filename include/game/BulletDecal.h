// A bullet-hole quad from the decal pool (hand-maintained; see decal.h).
#ifndef _GAME_BULLETDECAL_H_
#define _GAME_BULLETDECAL_H_

#include "types.h"
#include "game_types.h"

// One corner of a decal quad (0x18 bytes).
struct BulletDecalVertex {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ u8 r;
    /* 0x0D */ u8 g;
    /* 0x0E */ u8 b;
    /* 0x0F */ u8 a;
    /* 0x10 */ f32 u;
    /* 0x14 */ f32 v;
};

// size 0x68
class BulletDecal {
public:
    BulletDecal() {  // 0x80083F18 (weak)
        for (int i = 0; i < 4; i++) {
            m_verts[i].r = 0;
            m_verts[i].g = 0;
            m_verts[i].b = 0;
            m_verts[i].a = 0x80;
        }
    }

    /* 0x00 */ BulletDecalVertex m_verts[4];
    /* 0x60 */ BulletDecal* m_pNext;  // next in its texture bin
    /* 0x64 */ int m_type;
};

#endif
