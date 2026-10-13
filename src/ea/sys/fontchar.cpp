#include "ea/sys/font.h"

extern "C" FONTCHAR* FONT_bsearch(int code, const void* table, int count, int size) {
    while (count != 0) {
        FONTCHAR* ch = (FONTCHAR*)((const u8*)table + (count >> 1) * size);
        int diff = code - ch->code;
        if (diff == 0) {
            return ch;
        }
        if (diff > 0) {
            table = (const u8*)ch + size;
            count--;
        }
        count >>= 1;
    }
    return NULL;
}
