// EA "SHPG" shape files (GameCube texture container). Hand-maintained.
#ifndef _GAME_SHAPE_H_
#define _GAME_SHAPE_H_

#include "types.h"

// One image. The palette (for CI formats) is another record at `palette`
// bytes from the shape, with its data 0x10 bytes in.
struct Shape {
    /* 0x00 */ u32 format : 8;    // 0x14.. (see CTexture::Set); palettes use 0x20-0x32
    /* 0x00 */ s32 palette : 24;  // offset to the palette record, 0 if none
    /* 0x04 */ s16 width;
    /* 0x06 */ s16 height;
    /* 0x08 */ u8 unk_0x8[0x6];
    /* 0x0E */ u8 mipLevels : 4;  // mipmap count - 1
    /* 0x0E */ u8 unk_0xE : 4;
    /* 0x0F */ u8 unk_0xF;
    /* 0x10 */ u8 data[1];
};

struct ShapeFile {
    /* 0x00 */ char magic[4];  // "SHPG"
    /* 0x04 */ u8 unk_0x4[0x10];
    /* 0x14 */ s32 firstShape;  // offset of the first Shape
};

#endif
