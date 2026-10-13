#include "ea/sys/systemvars.h"
#include "ea/sys/printstr.h"
#include <stdarg.h>
#include <stdio.h>

extern "C" void REAL_exit(void);

const char* REALabortfilename;
int REALabortlinenum;

extern "C" void REAL_abortmessage(const char* fmt, ...) {
    char text[0x200];
    va_list args;

    if (fmt != NULL) {
        va_start(args, fmt);
        vsprintf(text, fmt, args);
    } else {
        text[0] = '\0';
    }
    REALabortmessage("%s", text);
}

extern "C" void SYSTEM_abortmessage(const char* fmt, ...) {
    char text[0x200];
    va_list args;

    if (fmt != NULL) {
        va_start(args, fmt);
        vsprintf(text, fmt, args);
    } else {
        text[0] = '\0';
    }
    PRINT_string(PRINTCHANNEL_2, "ERROR: %s", text);
    if (REALabortfilename != NULL) {
        PRINT_string(PRINTCHANNEL_2, "FILE %s LINE %d\n", REALabortfilename, REALabortlinenum);
    }
    REAL_exit();
}
