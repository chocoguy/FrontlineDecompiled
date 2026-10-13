#include "ea/sys/font.h"

#pragma auto_inline off
// The other-case letter of `c` (ASCII and Latin-1), 0 if none.
static int FONT_switchcase(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 0x20;
    }
    if (c >= 'a' && c <= 'z') {
        return c - 0x20;
    }
    if (c >= 0xC0 && c <= 0xD6) {
        return c + 0x20;
    }
    if (c >= 0xD8 && c <= 0xDE) {
        return c + 0x20;
    }
    if (c >= 0xE0 && c <= 0xF6) {
        return c - 0x20;
    }
    if (c >= 0xF8 && c <= 0xFE) {
        return c - 0x20;
    }
    return 0;
}
#pragma auto_inline reset

// Characters from 0x20 usually sit at index code - 0x20; otherwise search the sorted table.
static inline FONTCHAR* FONT_findchar(FONT* font, int c) {
    int size = 0xC;
    u8* table = (u8*)font + font->charOffset;
    if (font->flags & FONT_FLAG_WIDE) {
        size = 0x10;
    }
    int index = c - 0x20;
    if (index < font->numChars) {
        FONTCHAR* ch = (FONTCHAR*)(table + index * size);
        if (ch->code == c) {
            return ch;
        }
    }
    return FONT_bsearch(c, table, font->numChars, size);
}

template <typename T>
void FONT_drawtextx(FONT* font, float x, float y, const T* text, void* user);
template <typename T>
void FONT_getrectx(FONT* font, const T* text, float* rx, float* ry, float* rw, float* rh);

extern "C" void FONT_drawtexta(FONT* font, const char* text, float x, float y) {
    FONT_drawtextx(font, x, y, (const u8*)text, NULL);
}

extern "C" void FONT_getrecta(FONT* font, const char* text, float* x, float* y, float* w, float* h) {
    FONT_getrectx(font, (const u8*)text, x, y, w, h);
}

template <typename T>
void FONT_getrectx(FONT* font, const T* text, float* rx, float* ry, float* rw, float* rh) {
    float x = 0.0f;
    float y = 0.0f;
    float minX = 1e7f;
    float maxX = -1e7f;
    float minY = 1e7f;
    float maxY = -1e7f;
    float scaleX = font->scaleX;
    float scaleY = font->scaleY;
    T prev = 0;

    while (*text != 0) {
        FONTCHAR* ch = FONT_findchar(font, *text);
        if (ch == NULL) {
            if (*text == '\n') {
                prev = 0;
                text++;
                x = 0.0f;
                y += (int)(scaleY * (font->descent + font->ascent));
                continue;
            }
            int other = FONT_switchcase(*text);
            if (other != 0) {
                ch = FONT_findchar(font, other);
            }
            if (ch == NULL) {
                ch = FONT_findchar(font, 0x7F);
            }
            if (ch == NULL) {
                text++;
                continue;
            }
        }
        x += FONT_getkern(font, ch, prev) * scaleX;
        float left = x + ch->xOffset * scaleX;
        float top = y + ch->yOffset * scaleY;
        if (left < minX) {
            minX = left;
        }
        if (top < minY) {
            minY = top;
        }
        float right = left + ch->width * scaleX;
        float bottom = top + ch->height * scaleY;
        if (right > maxX) {
            maxX = right;
        }
        if (bottom > maxY) {
            maxY = bottom;
        }
        if (font->flags & FONT_FLAG_WIDE) {
            x += ch->advanceWide * scaleX;
        } else {
            x += ch->advance * scaleX;
        }
        prev = *text++;
    }

    if (rx != NULL) {
        *rx = maxX > minX ? minX : 0.0f;
    }
    if (ry != NULL) {
        *ry = maxY > minY ? minY : 0.0f;
    }
    if (rw != NULL) {
        *rw = maxX > minX ? maxX - minX : 0.0f;
    }
    if (rh != NULL) {
        *rh = maxY > minY ? maxY - minY : 0.0f;
    }
}

template <typename T>
void FONT_drawtextx(FONT* font, float x, float y, const T* text, void* user) {
    FONTDRAWCHAR chars[0x80];
    float startX = x;
    int count = 0;
    int first = 1;
    T prev = 0;

    if (FONTcurrentdriver->startDraw != NULL) {
        FONTcurrentdriver->startDraw(font);
    }
    float scaleX = font->scaleX;
    float scaleY = font->scaleY;

    while (*text != 0) {
        FONTCHAR* ch = FONT_findchar(font, *text);
        if (ch == NULL) {
            if (*text == '\n') {
                x = startX;
                prev = 0;
                text++;
                y += (int)(scaleY * (font->descent + font->ascent));
                continue;
            }
            int other = FONT_switchcase(*text);
            if (other != 0) {
                ch = FONT_findchar(font, other);
            }
            if (ch == NULL) {
                ch = FONT_findchar(font, 0x7F);
            }
            if (ch == NULL) {
                text++;
                continue;
            }
        }
        x += FONT_getkern(font, ch, prev) * scaleX;
        if (FONTcurrentdriverext.drawChars != NULL) {
            FONTDRAWCHAR* dc = &chars[count++];
            dc->ch = ch;
            dc->x = x;
            dc->y = y;
            if (count == 0x80) {
                if (user != NULL) {
                    FONTcurrentdriverext.drawCharsUser(font, chars, count, user, first);
                    first = 0;
                } else {
                    FONTcurrentdriverext.drawChars(font, chars, count);
                }
                count = 0;
            }
        } else {
            FONTcurrentdriver->draw(font, ch, x, y);
        }
        if (font->flags & FONT_FLAG_WIDE) {
            x += ch->advanceWide * scaleX;
        } else {
            x += ch->advance * scaleX;
        }
        prev = *text++;
    }

    if (count != 0) {
        if (user != NULL) {
            FONTcurrentdriverext.drawCharsUser(font, chars, count, user, first);
        } else {
            FONTcurrentdriverext.drawChars(font, chars, count);
        }
    }
    if (FONTcurrentdriver->endDraw != NULL) {
        FONTcurrentdriver->endDraw(font);
    }
}
