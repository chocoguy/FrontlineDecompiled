#include "ea/sys/font.h"

extern "C" int FONT_getkern(FONT* font, const FONTCHAR* ch, int prev) {
    int count = ch->kernCount;
    int kern = 0;

    if (count != 0) {
        FONTKERN* found;
        if (font->flags & FONT_FLAG_WIDE) {
            // The character's own list of preceding characters.
            FONTKERN* list = (FONTKERN*)((u8*)font + (font->kernOffset + ch->kernIndex * 4 + 4));
            int i;
            found = NULL;
            for (i = 0; i < count; i++) {
                if (prev == list[i].prev) {
                    found = &list[i];
                    break;
                }
            }
        } else {
            // One table of pairs for the whole font.
            s32* table = (s32*)((u8*)font + font->kernOffset);
            int n = table[0];
            FONTKERN* list = (FONTKERN*)(table + 1);
            int i;
            found = NULL;
            for (i = 0; i < n; i++) {
                if (prev == list[i].prev && ch->code == list[i].code) {
                    found = &list[i];
                    break;
                }
            }
        }
        if (found != NULL) {
            kern = found->amount;
        }
    }
    return kern;
}
