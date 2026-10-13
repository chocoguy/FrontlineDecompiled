// EA print library: formatted output by channel, sent to every enabled device.
#ifndef _EA_SYS_PRINTSTR_H_
#define _EA_SYS_PRINTSTR_H_

#include "types.h"

// Output channels (PRINTchannellist: "all", "real", "alert", "general", "memory", ...).
// 0 and 1 address every channel / every registered channel in PRINT_setchannelstate.
enum PRINTCHANNEL {
    PRINTCHANNEL_ALL = 0,
    PRINTCHANNEL_REAL = 1,
    PRINTCHANNEL_2 = 2,  // "alert": the default channel, the only one enabled
};

struct PRINTCHANNELENTRY {
    const char* name;
    u32 enabled : 1;
    u32 registered : 1;
};

// An output device: "print console" and "debugger" both go to OSReport.
struct PRINTDEVICE {
    const char* name;
    void (*func)(PRINTCHANNEL channel, const char* text);
    u32 enabled : 1;
};

extern PRINTDEVICE PRINTdevicelist[8];  // printn3.cpp
extern int PRINTinitialized;

extern "C" {
void PRINT_init(void);
void PRINT_string(PRINTCHANNEL channel, const char* fmt, ...);  // 0x801516E0
void PRINT_setdevicestate(int device, int state);
void PRINT_setchannelstate(PRINTCHANNEL channel, int state);
}
void PRINT_string(const char* fmt, ...);  // 0x8015176C: on PRINTCHANNEL_2

#endif
