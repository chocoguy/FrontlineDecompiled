#include "ea/sys/font.h"

// A font is its data: the driver just gets to set it up.
extern "C" FONT* FONT_create(const char* data) {
#ifdef FRONTLINE_NATIVE
    FontFromBigEndian((FONT*)data);
#endif
    if (FONTcurrentdriver->createFont != NULL) {
        FONTcurrentdriver->createFont((FONT*)data);
    }
    return (FONT*)data;
}

extern "C" void FONT_destroy(FONT* font) {
    if (FONTcurrentdriver->destroyFont != NULL) {
        FONTcurrentdriver->destroyFont(font);
    }
}
