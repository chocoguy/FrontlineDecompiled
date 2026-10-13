// CTexture: a GX texture object built from a shape. Hand-maintained.
#ifndef _GAME_TEXTURE_H_
#define _GAME_TEXTURE_H_

#include "types.h"
#include "game_types.h"
#include <dolphin/gx.h>

struct CDmaPacket;
class CRenderList;
struct Shape;
struct ShapeFile;

// size 0x40
class CTexture {
public:
    /* 0x00 */ void* m_pData;  // ShapeFile* once loaded (offset in a texture pack file)
    /* 0x04 */ u8 unk_0x4[0x4];
    /* 0x08 */ GXTexObj m_texObj;
    /* 0x28 */ GXTlutObj m_tlut;
    /* 0x34 */ int m_isCI;  // palettized
    /* 0x38 */ u8 unk_0x38[0x4];
    /* 0x3C */ s32 m_width : 10;
    /* 0x3C */ s32 m_height : 10;
    /* 0x3E */ s32 m_numMips : 4;
    /* 0x3F */ s32 unk_bits : 8;

    CTexture() {}  // 0x800A9234
    void Set(ShapeFile* file, bool);  // 0x80082D6C
    void Set(Shape* shape, bool);  // 0x80082AB0
    int Load(CDmaPacket&, CRenderList*);  // 0x80082A60
};

#endif
