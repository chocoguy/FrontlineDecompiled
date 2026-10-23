// IStudio text control: a string drawn with a font from the UI image library.
// Hand-maintained.
#ifndef _GAME_TIBTEXT_H_
#define _GAME_TIBTEXT_H_

#include "types.h"

struct UISParam_t;
struct UISString_t;

// TibTextGraphicDataT::align
#define TIBTEXT_ALIGN_CENTER 0x10
#define TIBTEXT_ALIGN_RIGHT 0x20

struct TibTextGraphicDataT {
    /* 0x00 */ PTR32(UISString_t) string;  // a file offset until loaded
    /* 0x04 */ u8 unk_0x4[0x4];
    /* 0x08 */ s16 fontIndex;  // in LibraryInfo[library]
    /* 0x0A */ s16 library;    // negative = hidden
    /* 0x0C */ u8 unk_0xC[0x4];
    /* 0x10 */ s32 align;
    /* 0x14 */ f32 color[4];  // r, g, b, a (0..255)
    /* 0x24 */ f32 color2[4];
    /* 0x34 */ f32 x;
    /* 0x38 */ f32 y;
    /* 0x3C */ u8 unk_0x3C[0x4];
    /* 0x40 */ f32 field_0x40[3];
    /* 0x4C */ s16 height;
};
FILE_SIZE_ASSERT(TibTextGraphicDataT, 0x50);

void TibTextDraw(TibTextGraphicDataT*);  // 0x8008C828
// The control's event handler (UISExecuteFnc): -1 load, -2 draw, -3 unload,
// 0..6 set properties from `params`.
void TibTextProcessFnc(void* data, unsigned long event, unsigned long numParams,
                       UISParam_t* params, UISParam_t* results);  // 0x8008CE84

#endif
