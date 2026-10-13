#include <dolphin/os.h>
#include <string.h>

void __OSSystemCallVectorStart(void);
void __OSSystemCallVectorEnd(void);

// Installed at the system call exception vector (0xC00): flushes the
// store-gather pipe by toggling HID0[ABE] and returns.
static asm void SystemCallVector(void) {
    nofralloc
entry __OSSystemCallVectorStart
    mfspr r9, HID0
    ori r10, r9, 0x8
    mtspr HID0, r10
    isync
    sync
    mtspr HID0, r9
    rfi
entry __OSSystemCallVectorEnd
    nop
}

void __OSInitSystemCall(void) {
    void* addr = (void*)0x80000C00;

    memcpy(addr, __OSSystemCallVectorStart, (u32)__OSSystemCallVectorEnd - (u32)__OSSystemCallVectorStart);
    DCFlushRangeNoSync(addr, 0x100);
    __sync();
    ICInvalidateRange(addr, 0x100);
}
