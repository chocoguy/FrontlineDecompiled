// CTexturePack: a texture pack file loaded into memory. Its pointers are file
// offsets until the constructor fixes them up. Hand-maintained.
#ifndef _GAME_TEXPACK_H_
#define _GAME_TEXPACK_H_

#include "types.h"

class CTexture;

typedef char TexPackName[16];

class CTexturePack {
public:
    /* 0x0 */ u8 unk_0x0[0x4];  // "TPAC"
    /* 0x4 */ u32 m_numTextures;
    /* 0x8 */ PTR32(TexPackName) m_names;  // sorted, case-insensitive
    /* 0xC */ PTR32(PTR32(CTexture)) m_textures;  // CTexture records in the file
    CTexturePack();  // 0x80082944
    CTexture* FindTexture(char* name);  // 0x800828B4
};
FILE_SIZE_ASSERT(CTexturePack, 0x10);

#ifdef FRONTLINE_NATIVE
#include "native/swap.h"

// .tpk files are big-endian GameCube data: the header, the texture offset
// table and each CTexture record's data offset. The shapes are swapped by
// CTexture::Set. Offsets are still file-relative here.
inline void TexturePackFromBigEndian(CTexturePack* pack) {
    u8* base = (u8*)pack;
    NativeSwap32s(base + 4, 3);
    NativeSwap32s(base + pack->m_textures.addr(), pack->m_numTextures);
    for (u32 i = 0; i < pack->m_numTextures; i++) {
        NativeSwap32(base + ((PTR32(CTexture)*)(base + pack->m_textures.addr()))[i].addr());  // m_pData
    }
}
#endif

#endif
