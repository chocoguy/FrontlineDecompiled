#include "ansi_files.h"

void __stdio_atexit(void) {
    __stdio_exit = __close_all;
}

void clearerr(FILE* file) {
    file->state.eof = 0;
    file->state.error = 0;
}
