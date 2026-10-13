#include "ea/real.h"
#include "dolphin/os.h"
#include <string.h>

struct CPUINFO {
    /* 0x00 */ int type;
    /* 0x04 */ u32 clock;
    /* 0x08 */ char name[0x10];
};

static CPUINFO cpuinfo;

extern "C" int CPU_detect(void) {
    cpuinfo.clock = OS_CORE_CLOCK;
    cpuinfo.type = 0;
    strcpy(cpuinfo.name, "Gekko PowerPC");
    return OS_CORE_CLOCK;
}
