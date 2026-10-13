#include "ea/sys/printstr.h"
#include <stdarg.h>
#include <stdio.h>

// The original calls these out of line.
#pragma auto_inline off

int PRINTinitialized;

static PRINTCHANNELENTRY PRINTchannellist[64] = {
    {"all", 1, 1},     {"real", 0, 1},   {"alert", 1, 1},      {"general", 0, 1},
    {"memory", 0, 1},  {"connection", 0, 1}, {"graphics", 0, 1}, {"file", 0, 1},
    {"input", 0, 1},   {"system", 0, 1}, {"3d", 0, 1},         {"font", 0, 1},
    {"codec", 0, 1},   {"math", 0, 1},   {"eagl", 0, 1},       {"capture", 0, 1},
};

static void PRINT_movechannel(PRINTCHANNEL channel, int state) {
    int i;
    if (channel == PRINTCHANNEL_ALL) {
        for (i = 0; i < 64; i++) {
            PRINTchannellist[i].enabled = (state != 0);
        }
    } else if (channel == PRINTCHANNEL_REAL) {
        for (i = 0; i < 64; i++) {
            if (PRINTchannellist[i].registered) {
                PRINTchannellist[i].enabled = (state != 0);
            }
        }
    } else {
        PRINTchannellist[channel].enabled = (state != 0);
    }
}

// Everything off, user channels and devices cleared; then "alert" to the print console.
extern "C" void PRINT_init(void) {
    int i;
    if (PRINTinitialized == 0) {
        PRINTinitialized = 1;
        for (i = 0; i < 8; i++) {
            PRINTdevicelist[i].enabled = 0;
        }
        for (i = 0; i < 64; i++) {
            PRINTchannellist[i].enabled = 0;
        }
        for (i = 4; i < 8; i++) {
            PRINTdevicelist[i].name = NULL;
            PRINTdevicelist[i].func = NULL;
        }
        for (i = 14; i < 64; i++) {
            PRINTchannellist[i].name = NULL;
            PRINTchannellist[i].registered = 0;
        }
        PRINT_setchannelstate(PRINTCHANNEL_2, 1);
        PRINT_setdevicestate(1, 1);
    }
}

static void PRINT_vstring(PRINTCHANNEL channel, const char* fmt, va_list args) {
    char text[0x2000];
    int i;

    if (PRINTinitialized == 0) {
        PRINT_init();
    }
    if (PRINTchannellist[channel].enabled) {
        vsprintf(text, fmt, args);
        for (i = 0; i < 8; i++) {
            if (PRINTdevicelist[i].enabled && PRINTdevicelist[i].func != NULL) {
                PRINTdevicelist[i].func(channel, text);
            }
        }
    }
}

extern "C" void PRINT_string(PRINTCHANNEL channel, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    PRINT_vstring(channel, fmt, args);
}

void PRINT_string(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    PRINT_vstring(PRINTCHANNEL_2, fmt, args);
}

extern "C" void PRINT_setdevicestate(int device, int state) {
    if (PRINTdevicelist[device].func != NULL) {
        if (PRINTinitialized == 0) {
            PRINT_init();
        }
        PRINTdevicelist[device].enabled = state;
    }
}

extern "C" void PRINT_setchannelstate(PRINTCHANNEL channel, int state) {
    if (PRINTinitialized == 0) {
        PRINT_init();
    }
    PRINT_movechannel(channel, state);
}
