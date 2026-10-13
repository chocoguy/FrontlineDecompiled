#include "game/texture.h"

#include <stdio.h>
#include <string.h>
#include "game/Moh2.h"
#include "game/shape.h"

// GX texture formats by shape format (0x14 + index).
#define SHAPE_FMT_RGB565 0x14
#define SHAPE_FMT_RGBA8 0x16
#define SHAPE_FMT_C4 0x18
#define SHAPE_FMT_C8 0x19
#define SHAPE_FMT_CMPR 0x1E

int CTexture::Load(CDmaPacket&, CRenderList*) {
    if (m_isCI != 0) {
        GXLoadTlut(&m_tlut, 0);
    }
    GXLoadTexObj(&m_texObj, 0);
    return 0;
}

// Note: unknown shape formats leave the texture format uninitialized, and a
// shape without a valid palette reads the palette type from address 0, as in
// the original.
void CTexture::Set(Shape* shape, bool) {
    u32 fmt;
    u32 ciFmt;
    u16 numEntries;
    u32 tlutFmt;
    bool mipmap = false;

    m_width = shape->width;
    m_height = shape->height;
    m_numMips = shape->mipLevels + 1;
    if (m_numMips > 1) {
        mipmap = true;
    }

    switch (shape->format) {
    case SHAPE_FMT_RGB565:
        fmt = 4;
        m_isCI = 0;
        break;
    case SHAPE_FMT_RGBA8:
        fmt = 6;
        m_isCI = 0;
        break;
    case SHAPE_FMT_C4:
        m_isCI = 1;
        ciFmt = 8;
        numEntries = 16;
        break;
    case SHAPE_FMT_C8:
        m_isCI = 1;
        ciFmt = 9;
        numEntries = 256;
        break;
    case SHAPE_FMT_CMPR:
        fmt = 0xE;
        m_isCI = 0;
        break;
    }

    if (m_isCI == 0) {
        GXInitTexObj(&m_texObj, shape->data, shape->width, shape->height, fmt, 0, 0, mipmap);
    } else {
        Shape* pal;
        if (shape->palette == 0) {
            pal = NULL;
        } else {
            pal = (Shape*)((u8*)shape + shape->palette);
            u8 type = pal->format;
            if (!(type == 0x20 || type == 0x21 || (u8)(type - 0x30) <= 1 || type == 0x32)) {
                pal = NULL;
            }
        }
        switch (pal->format) {
        case 0x30:
            tlutFmt = 0;
            break;
        case 0x31:
            tlutFmt = 1;
            break;
        case 0x32:
            tlutFmt = 2;
            break;
        }
        GXInitTlutObj(&m_tlut, pal->data, tlutFmt, numEntries);
        GXInitTexObjCI(&m_texObj, shape->data, shape->width, shape->height, ciFmt, 0, 0, mipmap, 0);
    }

    if (mipmap == true) {
        GXInitTexObjLOD(&m_texObj, m_isCI != 0 ? 3 : 5, 1, 0.0f, (float)m_numMips, 9.0f, 1, 1,
                        m_isCI == 0);
    }
    if (m_numMips > 1) {
        printf("Mipmaps! - [%d]\n", (int)m_numMips);
    }
}

void CTexture::Set(ShapeFile* file, bool unused) {
    if (strncmp(file->magic, "SHPG", 4) != 0) {
        DebugMsg("Non GAMECUBE shape format!\n");
        return;
    }
    Set((Shape*)((u8*)file + file->firstShape), unused);
}
