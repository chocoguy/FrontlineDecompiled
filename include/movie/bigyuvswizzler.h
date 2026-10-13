// tBigYUVSwizzler: draws a Y/U/V plane frame as RGB in one pass: lookup
// tables turn each plane's 8-bit samples into colour contributions and
// tBigSwizzler maps index the planes. Hand-maintained.
#ifndef _MOVIE_BIGYUVSWIZZLER_H_
#define _MOVIE_BIGYUVSWIZZLER_H_

#include "types.h"
#include "game_types.h"
#include "dolphin/gx.h"
#include "movie/bigswizzler.h"

// size 0x640 (allocated)
struct tBigYUVSwizzler {
    /* 0x000 */ u8 tlut[3][256][2];  // IA8 entries: Y, V and U contributions
    /* 0x600 */ tBigSwizzler* ySwizzler;
    /* 0x604 */ tBigSwizzler* uvSwizzler;
    /* 0x608 */ GXTlutObj tlutObj[3];
};

// Loads the plane textures (C8, each with its table) and sets up the TEV.
void tBigYUVSwizzler_DrawSetup(tBigYUVSwizzler* swizzler, GXTexObj* yTex, GXTexObj* vTex, GXTexObj* uTex);  // 0x800FC5AC
// The chroma swizzler is sized from vTex (U and V are the same size).
tBigYUVSwizzler* NEW_tBigYUVSwizzlerTexture(GXTexObj* yTex, GXTexObj* vTex, GXTexObj* uTex);  // 0x800FCC14
void DELETE_tBigYUVSwizzler(tBigYUVSwizzler* swizzler);  // 0x800FCE14

#endif
