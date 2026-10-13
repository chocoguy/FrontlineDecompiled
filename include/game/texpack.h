// CTexturePack: a texture pack file loaded into memory. Its pointers are file
// offsets until the constructor fixes them up. Hand-maintained.
#ifndef _GAME_TEXPACK_H_
#define _GAME_TEXPACK_H_

#include "types.h"

class CTexture;

class CTexturePack {
public:
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ u32 m_numTextures;
    /* 0x8 */ char (*m_names)[16];  // sorted, case-insensitive
    /* 0xC */ CTexture** m_textures;
    CTexturePack();  // 0x80082944
    CTexture* FindTexture(char* name);  // 0x800828B4
};

#endif
