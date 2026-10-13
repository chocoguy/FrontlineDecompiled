// tBigSwizzler: a pair of IA8 indirect lookup textures that let GX sample
// a texture bigger than one tile row as if its blocks were linear (the
// movie frames' Y/U/V planes). Hand-maintained.
#ifndef _MOVIE_BIGSWIZZLER_H_
#define _MOVIE_BIGSWIZZLER_H_

#include "types.h"
#include "game_types.h"
#include "dolphin/gx.h"

// size 0x60
struct tBigSwizzler {
    /* 0x00 */ u32 blockWidth;  // the source format's texel block
    /* 0x04 */ u32 blockHeight;
    /* 0x08 */ u32 tileWidth;  // the tile map: which block a texel is in
    /* 0x0C */ u32 tileHeight;
    /* 0x10 */ u8* tileData;
    /* 0x14 */ GXTexObj tileTex;
    /* 0x34 */ u32 pixWidth;  // the pixel map: the offset within the block
    /* 0x38 */ u32 pixHeight;
    /* 0x3C */ u8* pixData;
    /* 0x40 */ GXTexObj pixTex;
};

tBigSwizzler* NEW_tBigSwizzlerTexture(GXTexObj* tex);  // 0x800FC198
void DELETE_tBigSwizzler(tBigSwizzler* swizzler);  // 0x800FC56C

#endif
