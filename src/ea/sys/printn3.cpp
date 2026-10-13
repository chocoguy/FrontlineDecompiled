#include "ea/sys/printstr.h"
#include "dolphin/os.h"

static void PRINT_debuggerfunc(PRINTCHANNEL, const char* text) {
    OSReport("%s", text);
}

PRINTDEVICE PRINTdevicelist[8] = {
    {"", NULL, 0},
    {"print console", PRINT_debuggerfunc, 0},
    {"debugger", PRINT_debuggerfunc, 0},
};
