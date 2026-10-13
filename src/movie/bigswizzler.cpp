// tBigSwizzler: builds the two IA8 indirect lookup maps that address a
// texture by linear position. Both maps are 4x4-tiled like any GX IA8
// texture; each texel holds the coordinates (in blocks for the tile map,
// within the block for the pixel map) of the source texel to sample.
#include "movie/bigswizzler.h"
#include "movie/rcmp.h"
#include "dolphin/os.h"
#include "ea/real.h"

// The byte offset of texel (x, y) in a 4x4-tiled 16-bit texture `width`
// texels wide.
static inline int IA8_Offset(u32 x, u32 y, u32 width) {
    return ((y & 3) << 3) + (((x & 3) << 1) + ((((int)x >> 2) + ((int)y >> 2) * ((int)(width + 3) >> 2)) << 5));
}

tBigSwizzler* NEW_tBigSwizzlerTexture(GXTexObj* tex) {
    u32 blockW = 0;
    u32 blockH = 0;
    u32 width = GXGetTexObjWidth(tex);
    OSReport("NEW_tBigSwizzlerTexture %d,%d\n", width, GXGetTexObjHeight(tex));

    switch (GXGetTexObjFmt(tex)) {
    case 0:  // I4
    case 8:  // C4
        blockW = 8;
        blockH = 8;
        break;
    case 1:  // I8
    case 2:  // IA4
    case 9:  // C8
        blockW = 8;
        blockH = 4;
        break;
    case 3:  // IA8
    case 4:  // RGB565
    case 5:  // RGB5A3
        blockW = 4;
        blockH = 4;
        break;
    }

    u32 blocksX = (blockW + width - 1) / blockW;
    u32 blockSize = blockW * blockH;

    tBigSwizzler* sw = (tBigSwizzler*)MEM_alloc("tBigSwizzler", sizeof(tBigSwizzler), RCMP::rcmp_sys.m_allocParam);
    sw->blockWidth = blockW;
    sw->blockHeight = blockH;
    sw->tileWidth = width;
    sw->tileHeight = blockH;
    sw->tileData = (u8*)MEM_alloc("tBigSwizzler::TileMap", sw->tileWidth * sw->tileHeight * 2,
                                  RCMP::rcmp_sys.m_allocParam);
    sw->pixWidth = width;
    sw->pixHeight = blockH;
    sw->pixData = (u8*)MEM_alloc("tBigSwizzler::PixMap", sw->pixWidth * sw->pixHeight * 2,
                                 RCMP::rcmp_sys.m_allocParam);

    u32 x, y, pos;
    for (y = 0; y < sw->tileHeight; y++) {
        pos = y * width;
        for (x = 0; x < sw->tileWidth; x++, pos++) {
            u32 block = pos / blockSize;
            u32 within = pos % blockSize;
            u8* p = (u8*)&((u16*)sw->tileData)[IA8_Offset(x, y, sw->pixWidth) / 2];
            p[0] = (x % blockW + (block % blocksX) * blockW) / blockW;
            p[1] = (within / blockW + (y / blockH) * blockH) / blockH;
        }
    }
    GXInitTexObj(&sw->tileTex, sw->tileData, sw->tileWidth, sw->tileHeight, 3, 0, 1, 0);  // IA8, clamp/repeat
    GXInitTexObjLOD(&sw->tileTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);

    for (y = 0; y < sw->pixHeight; y++) {
        pos = y * width;
        for (x = 0; x < sw->pixWidth; x++, pos++) {
            u32 block = pos / blockSize;
            u32 within = pos % blockSize;
            u8* p = (u8*)&((u16*)sw->pixData)[IA8_Offset(x, y, sw->pixWidth) / 2];
            p[0] = (x % blockW + (block % blocksX) * blockW) % blockW;
            p[1] = (within / blockW + (y / blockH) * blockH) % blockH - y + 0x80;
        }
    }
    GXInitTexObj(&sw->pixTex, sw->pixData, sw->pixWidth, sw->pixHeight, 3, 0, 1, 0);
    GXInitTexObjLOD(&sw->pixTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);

    DCFlushRange(sw->tileData, sw->tileWidth * sw->tileHeight * 2);
    DCFlushRange(sw->pixData, sw->pixWidth * sw->pixHeight * 2);
    return sw;
}

void DELETE_tBigSwizzler(tBigSwizzler* sw) {
    MEM_free(sw->tileData);
    MEM_free(sw->pixData);
    MEM_free(sw);
}
