// Native (little-endian) build only: byte-swaps a UI screen file (.scr).
//
// A screen file is the IStudio compiler's output: the header and the
// structures PatchScrData relocates, control state, script variables and
// plugin objects, string text, then the script code. Script code is read a
// byte at a time (UISStack.c's READ_U32 assembles big-endian values), so it
// stays as it is; nothing but code follows the first event handler. Below it,
// everything the CPU reads is a 32-bit word, except the records with 16-bit
// fields or bytes, which are swapped by type here:
//   event and list entries     u16, u16, u32
//   TibPolygon objects (0)     4 x s16, then 4 vertices of 5 floats and 4 color bytes
//   TibText objects (7)        s16 pairs at 0x08 and 0x4C
//   string text                bytes
// Screens can be loaded again from the same (cached) data, so the swap checks
// that it hasn't run: numControls reads as n << 24 while still big-endian.
#ifndef _GAME_UISSWAP_H_
#define _GAME_UISSWAP_H_

#ifdef FRONTLINE_NATIVE
#include <stdlib.h>

#include "game/TibPolygon.h"
#include "game/TibText.h"
#include "game/uistypes.h"
#include "native/swap.h"

inline u32 ScrLoadBE(const u8* base, u32 off) {
    return __builtin_bswap32(NativeLoad32(base + off));
}

// Swaps a screen file in place, before PatchScrData. Returns false if it was
// already swapped.
inline bool ScrDataFromBigEndian(UISScrData_t* data) {
    u8* base = (u8*)data;
    if (data->numControls < 0x10000) {
        return false;
    }
    NativeSwap32s(base, sizeof(UISScrData_t) / 4);

    // Where the code starts: the first event handler.
    u32 codeStart = 0xFFFFFFFF;
    for (u32 i = 0; i < data->numControls; i++) {
        const u8* control = base + data->controls.addr() + i * sizeof(UISControl_t);
        u32 numEvents = ScrLoadBE(control, 0xC);
        u32 events = ScrLoadBE(control, 0x10);
        for (u32 k = 0; k < numEvents; k++) {
            const u8* e = base + events + k * sizeof(UISEventEntry_t);
            if ((u16)((e[2] << 8) | e[3]) != UISEVENT_CHILD) {
                u32 pc = ScrLoadBE(e, 4);
                if (pc < codeStart) {
                    codeStart = pc;
                }
            }
        }
    }

    if (codeStart == 0xFFFFFFFF) {
        __builtin_trap();  // no script code to end the data (every screen on the disc has some)
    }

    // One flag per word below the code: set once a word has been swapped (or
    // must not be, as in string text).
    u32 numWords = codeStart / 4;
    u8* done = (u8*)calloc(numWords + 1, 1);
    for (u32 w = 0; w < sizeof(UISScrData_t) / 4; w++) {
        done[w] = 1;
    }
    struct Words {
        u8* base;
        u8* done;
        u32 numWords;
        // Bytes [off, off + size) are handled: don't swap them as words.
        void claim(u32 off, u32 size) {
            for (u32 w = off / 4; w < (off + size + 3) / 4 && w < numWords; w++) {
                done[w] = 1;
            }
        }
        void swap32(u32 off) {
            if (off / 4 < numWords && !done[off / 4]) {
                NativeSwap32(base + off);
                done[off / 4] = 1;
            }
        }
        // A u16 pair: the word at `off` once, as two halves.
        void swap16x2(u32 off) {
            if (off / 4 < numWords && !done[off / 4]) {
                NativeSwap16s(base + off, 2);
                done[off / 4] = 1;
            }
        }
    } words = {base, done, numWords};

    // List and event entries: a u16 pair, then a word.
    struct Entries {
        static void Swap(Words& words, u32 off, u32 count) {
            for (u32 k = 0; k < count; k++) {
                words.swap16x2(off + k * 8);
                words.swap32(off + k * 8 + 4);
            }
        }
    };
    // A plugin object, by type (see IStudio::ISirenStartup).
    struct Objects {
        static void Swap(Words& words, u16 type, u32 off) {
            if (type == 0) {
                words.swap16x2(off + offsetof(TibPolyData_t, imageIndex));
                words.swap16x2(off + offsetof(TibPolyData_t, flags));
                for (u32 v = 0; v < 4; v++) {
                    u32 vert = off + offsetof(TibPolyData_t, verts) + v * sizeof(TibPolyVert_t);
                    for (u32 f = 0; f < 5; f++) {
                        words.swap32(vert + f * 4);
                    }
                    words.claim(vert + offsetof(TibPolyVert_t, color), 4);
                }
            } else if (type == 7) {
                words.swap16x2(off + offsetof(TibTextGraphicDataT, fontIndex));
                words.swap16x2(off + offsetof(TibTextGraphicDataT, height));
            }
            // The other words go with the rest.
        }
    };

    Entries::Swap(words, data->objects.addr(), data->numObjects);
    for (u32 i = 0; i < data->numObjects; i++) {
        UISListEntry_t* e = (UISListEntry_t*)(base + data->objects.addr()) + i;
        if (e->data != 0) {
            Objects::Swap(words, e->type, e->data);
        }
    }
    for (u32 i = 0; i < data->numControls; i++) {
        u32 control = data->controls.addr() + i * sizeof(UISControl_t);
        for (u32 f = 0; f < 5; f++) {
            words.swap32(control + f * 4);
        }
        UISControl_t* c = (UISControl_t*)(base + control);
        Entries::Swap(words, c->events.addr(), c->numEvents);
        for (u32 j = 0; j < c->numLists; j++) {
            words.swap32(c->lists.addr() + j * 4);
            u32 list = ((PTR32(UISControlList_t)*)(base + c->lists.addr()))[j].addr();
            for (u32 f = 0; f < 3; f++) {
                words.swap32(list + f * 4);
            }
            UISControlList_t* l = (UISControlList_t*)(base + list);
            Entries::Swap(words, l->entries.addr(), l->numEntries);
            for (u32 k = 0; k < l->numEntries; k++) {
                UISListEntry_t* e = (UISListEntry_t*)(base + l->entries.addr()) + k;
                if (e->type != UISLIST_CONTROL) {
                    Objects::Swap(words, e->type, e->data);
                }
            }
        }
    }
    for (u32 i = 0; i < data->numStrings; i++) {
        u32 str = data->strings.addr() + i * sizeof(UISString_t);
        for (u32 f = 0; f < 3; f++) {
            words.swap32(str + f * 4);
        }
        UISString_t* s = (UISString_t*)(base + str);
        words.claim(s->text.addr(), s->maxLength + 1);  // and its terminator
    }

    // Everything else: control state, variables, the string reference table.
    for (u32 w = 0; w < numWords; w++) {
        if (!done[w]) {
            NativeSwap32(base + w * 4);
        }
    }
    free(done);
    return true;
}
#endif

#endif
