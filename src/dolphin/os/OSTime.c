#include <dolphin/os.h>

#define OS_SYSTEM_TIME_ADJUST (*(OSTime*)0x800030D8)

typedef u32 OSTick;

asm OSTime OSGetTime(void) {
    nofralloc
jump:
    mftbu r3
    mftb r4
    mftbu r5
    cmpw r3, r5
    bne jump
    blr
}

asm OSTick OSGetTick(void) {
    nofralloc
    mftb r3
    blr
}

OSTime __OSGetSystemTime(void) {
    BOOL enabled;
    OSTime result;

    enabled = OSDisableInterrupts();
    result = OS_SYSTEM_TIME_ADJUST + OSGetTime();
    OSRestoreInterrupts(enabled);
    return result;
}

OSTime __OSTimeToSystemTime(OSTime time) {
    BOOL enabled;
    OSTime result;

    enabled = OSDisableInterrupts();
    result = OS_SYSTEM_TIME_ADJUST + time;
    OSRestoreInterrupts(enabled);
    return result;
}
