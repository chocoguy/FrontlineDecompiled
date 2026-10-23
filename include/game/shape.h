// EA "SHPG" shape files (GameCube texture container). Hand-maintained.
#ifndef _GAME_SHAPE_H_
#define _GAME_SHAPE_H_

#include "types.h"

// One image. The palette (for CI formats) is another record at `palette`
// bytes from the shape, with its data 0x10 bytes in.
struct Shape {
#ifdef FRONTLINE_NATIVE
    // clang allocates bitfields from the low bit, CodeWarrior from the high
    // bit: declared in reverse so the (swapped) file word's bits line up.
    /* 0x00 */ s32 palette : 24;  // offset to the palette record, 0 if none
    /* 0x00 */ u32 format : 8;    // 0x14.. (see CTexture::Set); palettes use 0x20-0x32
#else
    /* 0x00 */ u32 format : 8;    // 0x14.. (see CTexture::Set); palettes use 0x20-0x32
    /* 0x00 */ s32 palette : 24;  // offset to the palette record, 0 if none
#endif
    /* 0x04 */ s16 width;
    /* 0x06 */ s16 height;
    /* 0x08 */ u8 unk_0x8[0x6];
#ifdef FRONTLINE_NATIVE
    /* 0x0E */ u8 unk_0xE : 4;
    /* 0x0E */ u8 mipLevels : 4;  // mipmap count - 1
#else
    /* 0x0E */ u8 mipLevels : 4;  // mipmap count - 1
    /* 0x0E */ u8 unk_0xE : 4;
#endif
    /* 0x0F */ u8 unk_0xF;
    /* 0x10 */ u8 data[1];
};

struct ShapeFile {
    /* 0x00 */ char magic[4];  // "SHPG"
    /* 0x04 */ u8 unk_0x4[0x10];
    /* 0x14 */ s32 firstShape;  // offset of the first Shape
};
FILE_SIZE_ASSERT(ShapeFile, 0x18);

// The first byte of a shape or palette record: its format.
#ifdef FRONTLINE_NATIVE
#define SHAPE_TYPE(record) (((Shape*)(record))->format)
#else
#define SHAPE_TYPE(record) (*(u8*)(record))
#endif

#ifdef FRONTLINE_NATIVE
#include "native/swap.h"

// Shape files are big-endian. Swaps the header fields the CPU reads (the
// first shape and its palette record); the pixels and palette entries are
// read by the GPU and stay big-endian. Files can be shared, so a swapped one
// is marked by its magic becoming "SHPN".
//
// A shape or palette record's header: the format/offset word, width, height.
inline void ShapeFromBigEndian(u8* shape) {
    NativeSwap32(shape);
    NativeSwap16(shape + 4);
    NativeSwap16(shape + 6);
    if (((Shape*)shape)->palette != 0) {
        u8* palette = shape + ((Shape*)shape)->palette;
        NativeSwap32(palette);
        NativeSwap16(palette + 4);
        NativeSwap16(palette + 6);
    }
}

inline ShapeFile* ShapeFileFromBigEndian(ShapeFile* file) {
    if (file == NULL || file->magic[3] != 'G') {
        return file;
    }
    file->magic[3] = 'N';
    NativeSwap32(&file->firstShape);
    ShapeFromBigEndian((u8*)file + file->firstShape);
    return file;
}
#endif

#endif
