// EA bitmap font library (FONT_*): layout of its font state as the game uses
// it, and the render driver the game installs.
#ifndef _EA_SYS_FONT_H_
#define _EA_SYS_FONT_H_

#include "types.h"

// Font flag: wide character records (FONTCHAR is 0x10 bytes, with per-character kern lists
// and 16-bit advances); otherwise 0xC bytes and one kern table for the whole font.
#define FONT_FLAG_WIDE 0x40000

// One glyph in a font's character table (0xC bytes, 0x10 with FONT_FLAG_WIDE).
struct FONTCHAR {
    /* 0x0 */ u16 code;
    /* 0x2 */ u8 width;
    /* 0x3 */ u8 height;
    /* 0x4 */ u16 texX;  // position in the font texture
    /* 0x6 */ u16 texY;
    /* 0x8 */ s8 advance;
    /* 0x9 */ s8 xOffset;
    /* 0xA */ s8 yOffset;
    /* 0xB */ u8 kernCount;   // wide: entries in this character's kern list
    /* 0xC */ u16 kernIndex;  // wide only: first kern entry
    /* 0xE */ s16 advanceWide;  // wide only
};

// Kern pair: `amount` pixels added before `code` when it follows `prev`.
// Wide fonts keep a list per character (only `prev` is compared).
struct FONTKERN {
    /* 0x0 */ u16 prev;
    /* 0x2 */ s8 amount;
    /* 0x3 */ u8 code;
};

// One queued character for the batch drawing driver.
struct FONTDRAWCHAR {
    /* 0x0 */ const FONTCHAR* ch;
    /* 0x4 */ f32 x;
    /* 0x8 */ f32 y;
};

// A loaded font (a font file in memory).
struct FONT {
    /* 0x00 */ u8 unk_0x0[0xA];
    /* 0x0A */ u16 numChars;
    /* 0x0C */ u32 flags;  // FONT_FLAG_*
    /* 0x10 */ u8 unk_0x10[0x2];
    /* 0x12 */ u8 ascent;   // the line height is ascent + descent
    /* 0x13 */ u8 descent;
    /* 0x14 */ s32 charOffset;  // FONTCHAR table at FONT + charOffset, sorted by code
    /* 0x18 */ s32 kernOffset;  // kern data at FONT + kernOffset (s32 count, then FONTKERNs)
    /* 0x1C */ s32 shapeOffset;  // the font texture (a Shape) at FONT + shapeOffset
    /* 0x20 */ u32 color;        // CColor bits
    /* 0x24 */ f32 height;
    /* 0x28 */ f32 depth;        // stored as raw bits of an integer (CFont::SetDepth)
    /* 0x2C */ u8 unk_0x2C[0xC];
    /* 0x38 */ f32 scaleX;
    /* 0x3C */ f32 scaleY;
    /* 0x40 */ u8 unk_0x40[0x30];
    /* 0x70 */ void* user;  // the CFont drawing
};

// Callbacks the library uses to draw.
struct FONTDRIVER {
    void (*draw)(FONT* font, const FONTCHAR* ch, float x, float y);
    void (*startDraw)(FONT* font);
    void (*endDraw)(FONT* font);
    void (*createFont)(FONT* font);
    void (*destroyFont)(FONT* font);
};

// Optional batch drawing hooks (all null in this game).
struct FONTDRIVEREXT {
    void* unk0;
    void (*drawChars)(FONT* font, FONTDRAWCHAR* chars, int count);
    void* unk8;
    void (*drawCharsUser)(FONT* font, FONTDRAWCHAR* chars, int count, void* user, int first);
    void* unk10[3];
};

extern FONTDRIVER* FONTcurrentdriver;
extern FONTDRIVEREXT FONTcurrentdriverext;  // fontdriver.cpp  // fontdriver.cpp; FONTnulldriver until one is installed
extern FONTDRIVER FONTnulldriver;  // fontnull.cpp
extern FONT* FONTstand;  // the built-in font (FONT_init)
extern u8 FONTstandfile[0x1CF0];  // fontstdf.cpp: its data

extern "C" {
void FONT_restore(void);  // 0x8014E334
void FONT_init(void);  // 0x8014E374
FONT* FONT_create(const char* data);  // 0x8014E3B8
void FONT_destroy(FONT* font);  // 0x8014E3FC
void FONT_installdriver(FONTDRIVER* driver);  // 0x8014E430
// Binary search of a table of `count` records of `size` bytes starting with a u16 code.
FONTCHAR* FONT_bsearch(int code, const void* table, int count, int size);  // 0x8014E438
// Kerning of `ch` after the character `prev`.
int FONT_getkern(FONT* font, const FONTCHAR* ch, int prev);  // 0x8014EDB0
void FONT_drawtexta(FONT* font, const char* text, float x, float y);  // 0x8014E518
// Bounding rectangle of `text` in `font`.
void FONT_getrecta(FONT* font, const char* text, float* x, float* y, float* w, float* h);  // 0x8014E53C
}

#endif
