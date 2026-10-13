#include "ea/sys/font.h"
#include "ea/sys/exit.h"

FONT* FONTstand;

extern "C" void FONT_restore(void) {
    if (FONTstand != NULL) {
        FONT_destroy(FONTstand);
        FONTstand = NULL;
        REAL_removeexit(FONT_restore);
    }
}

extern "C" void FONT_init(void) {
    if (FONTstand == NULL) {
        FONTstand = FONT_create((const char*)FONTstandfile);
        REAL_addexit(FONT_restore);
    }
}
