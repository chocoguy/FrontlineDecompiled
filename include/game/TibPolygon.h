// IStudio polygon control: a quad, flat colored or textured with an image
// from the UI image library. Hand-maintained.
#ifndef _GAME_TIBPOLYGON_H_
#define _GAME_TIBPOLYGON_H_

#include "types.h"

struct UISParam_t;

// TibPolyData_t::flags
#define TIBPOLY_TINTED 0x1    // apply vertex colors and UI tint even when textured
#define TIBPOLY_TEXTURED 0x2

struct TibPolyVert_t {
    /* 0x00 */ f32 u;
    /* 0x04 */ f32 v;
    /* 0x08 */ f32 x;
    /* 0x0C */ f32 y;
    /* 0x10 */ f32 z;
    /* 0x14 */ u8 color[4];  // r, g, b, a
};

struct TibPolyData_t {
    /* 0x00 */ s16 imageIndex;  // in LibraryInfo[library]
    /* 0x02 */ s16 library;     // negative = none
    /* 0x04 */ s16 flags;       // TIBPOLY_*
    /* 0x06 */ s16 field_0x6;
    /* 0x08 */ TibPolyVert_t verts[4];
};

void TibPolyDraw(TibPolyData_t*);  // 0x8008BCD4
// Loads the polygon's image if needed.
void TibPolyInit(TibPolyData_t*);  // 0x8008BFEC
// The control's event handler: -1 load, -2 draw, -3 unload, 0..5 set properties.
void TibPolyProcessFnc(void* data, unsigned long event, unsigned long numParams,
                       UISParam_t* params, UISParam_t* results);  // 0x8008C630

#ifdef FRONTLINE_NATIVE
class CSprite;

// The button icons for the device in use (FrontlineRewritten's
// game/native/hint_icons.cpp): Loaded gives an icon image a keyboard twin,
// NativeHintIcon picks the one to draw, Unloaded drops the twin.
void NativeHintIconLoaded(const char* name, CSprite* sprite);
void NativeHintIconUnloaded(CSprite* sprite);
CSprite* NativeHintIcon(CSprite* sprite);
#endif

#endif
