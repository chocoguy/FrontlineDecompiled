// IStudio polygon control.
#include "game/TibPolygon.h"
#include "game/ShellMenu.h"
#include "game/Screen.h"
#include "game/UISUtils.h"
#include "game/isexportdefs.h"
#include "game/matstack.h"
#include "game/sprite.h"
#include "game/uistypes.h"
#include "string.h"

// Vertex color channel: half of the tinted value, at most 0x7F.
static inline u8 TintChannel(u8 value, float mul, float add) {
    u8 c = 0.5f * (mul * (value + add));
    if (c > 0x7F) {
        c = 0x7F;
    }
    return c;
}

void TibPolyProcessFnc(void* p, unsigned long event, unsigned long numParams,
                       UISParam_t* params, UISParam_t* results) {
    TibPolyData_t* data = (TibPolyData_t*)p;

    switch (event) {
    case (unsigned long)-1:
        TibPolyInit(data);
        break;
    case (unsigned long)-2:
        TibPolyDraw(data);
        break;
    case (unsigned long)-3:
        if (data->library >= 0) {
            CSprite* sprite = LibraryInfo[data->library].images[data->imageIndex];
            if (sprite != NULL) {
#ifdef FRONTLINE_NATIVE
                NativeHintIconUnloaded(sprite);
#endif
                delete sprite;
                LibraryInfo[data->library].images[data->imageIndex] = NULL;
            }
        }
        break;
    case 0: {
        TibPolyVert_t* v = &data->verts[(s32)params[0].value];
        v->x = *(f32*)&params[1].value;
        v->y = *(f32*)&params[2].value;
        break;
    }
    case 1: {
        TibPolyVert_t* v = &data->verts[(s32)params[0].value];
        v->u = *(f32*)&params[1].value;
        v->v = *(f32*)&params[2].value;
        break;
    }
    case 2: {
        // params: vertex (-1 = all), r, g, b
        s32 which = params[0].value;
        u32 r = params[1].value;
        u32 g = params[2].value;
        u32 b = params[3].value;
        if (which == -1) {
            for (int i = 3; i >= 0; i--) {
                data->verts[i].color[0] = r;
                data->verts[i].color[2] = b;
                data->verts[i].color[1] = g;
            }
            break;
        }
        data->verts[which].color[0] = r;
        data->verts[which].color[2] = b;
        data->verts[which].color[1] = g;
        break;
    }
    case 3: {
        // params: vertex (-1 = all), alpha
        s32 which = params[0].value;
        u32 a = params[1].value;
        if (which == -1) {
            for (int i = 3; i >= 0; i--) {
                data->verts[i].color[3] = a;
            }
            break;
        }
        data->verts[which].color[3] = a;
        break;
    }
    case 4:
        data->field_0x6 = params[0].value;
        break;
    case 5:
        // params: image (low 16 bits) and library (high 16 bits), or library, image
        if (numParams == 1) {
            data->imageIndex = params[0].value;
            data->library = params[0].value >> 16;
        } else {
            data->imageIndex = params[1].value;
            data->library = params[0].value;
        }
        TibPolyInit(data);
        break;
    }
}

// Loads the image with a render priority chosen by library and name.
void TibPolyInit(TibPolyData_t* data) {
    if (!(data->flags & TIBPOLY_TEXTURED) || data->library < 0) {
        return;
    }
    IStudioLibrary* lib = &LibraryInfo[data->library];
    if (lib->images[data->imageIndex] != NULL) {
        return;
    }

    CSprite* sprite;
#define NAME LibraryInfo[data->library].imageNames[data->imageIndex]
#define LOAD(type, prio) new CSprite(NAME, (CSprite::ESpriteLoadType)(type), (ERenderPriority)(prio))
    if (data->library == 0xB) {
        sprite = LOAD(3, 0xB);
    } else if (strstr(lib->imageNames[data->imageIndex], "master") != NULL ||
               strstr(NAME, "master") != NULL) {
        sprite = LOAD(2, 0xB);
    } else if (data->library == 0xC) {
        sprite = LOAD(2, 0x5C);
    } else if (data->library == 0xD) {
        if (strstr(NAME, "Logo") != NULL) {
            sprite = LOAD(2, 0x52);
        } else if (strstr(NAME, "HL") != NULL) {
            sprite = LOAD(2, 0x48);
        } else if (strstr(NAME, "pistol") != NULL) {
            sprite = LOAD(2, 0x48);
        } else if (strstr(NAME, "TXT") != NULL) {
            sprite = LOAD(2, 0x3E);
        } else if (strstr(NAME, "can") != NULL) {
            sprite = LOAD(2, 0x34);
        } else if (strstr(NAME, "case") != NULL) {
            sprite = LOAD(2, 0x2A);
        } else if (strstr(NAME, "folder") != NULL) {
            sprite = LOAD(2, 0x20);
        } else if (strstr(NAME, "smoke_") != NULL) {
            sprite = LOAD(2, 0x25);
        } else {
            sprite = LOAD(2, 0xC);
        }
    } else {
        sprite = LOAD(2, 0xC);
    }
#undef LOAD
#undef NAME
    LibraryInfo[data->library].images[data->imageIndex] = sprite;
#ifdef FRONTLINE_NATIVE
    NativeHintIconLoaded(lib->imageNames[data->imageIndex], sprite);
#endif
}

void TibPolyDraw(TibPolyData_t* data) {
    if (g_Shell.field_0x19A7 && !g_Shell.field_0x199B && data->library == 0xC) {
        return;
    }
    float* add = UISGetColorAdditive();
    float* mul = UISGetColorMultipler();

    _spritepolyvert verts[4];
    TibPolyVert_t* src = data->verts;
    _spritepolyvert* dst = verts;
    for (int i = 0; i < 4; i++, src++, dst++) {
        CVector3 pos;
        CVector3 in;
        in.x = src->x;
        in.y = src->y;
        in.z = src->z;
        g_matStack->Top()->TransformPoint(pos, in);
        dst->x = pos.x * g_screen.m_widthScale;
        dst->y = pos.y * g_screen.m_heightScale;
        dst->z = pos.z;
        if ((data->flags & TIBPOLY_TINTED) || !(data->flags & TIBPOLY_TEXTURED)) {
            dst->color[0] = TintChannel(src->color[0], mul[0], add[0]);
            dst->color[1] = TintChannel(src->color[1], mul[1], add[1]);
            dst->color[2] = TintChannel(src->color[2], mul[2], add[2]);
            dst->color[3] = TintChannel(src->color[3], mul[3], add[3]);
        } else {
            dst->color[0] = 0x7F;
            dst->color[1] = 0x7F;
            dst->color[2] = 0x7F;
            dst->color[3] = 0x7F;
        }
        dst->u = src->u;
        dst->v = src->v;
    }

    // The quad always maps the whole image (the vertex UVs are ignored).
    verts[0].u = 0.0f;
    verts[1].u = 1.0f;
    verts[2].u = 1.0f;
    verts[3].u = 0.0f;
    verts[0].v = 0.0f;
    verts[1].v = 0.0f;
    verts[2].v = 1.0f;
    verts[3].v = 1.0f;

    if (data->flags & TIBPOLY_TEXTURED) {
        if (data->library >= 0) {
#ifdef FRONTLINE_NATIVE
            NativeHintIcon(LibraryInfo[data->library].images[data->imageIndex])
                ->DrawPoly(verts, 4, (CSprite::EPolyPrimType)5);
#else
            LibraryInfo[data->library].images[data->imageIndex]->DrawPoly(
                verts, 4, (CSprite::EPolyPrimType)5);
#endif
        }
    } else {
        CSprite::DrawPolyNoTex(verts, 4, (CSprite::EPolyPrimType)5);
    }
}
