#include "ea/sys/mem.h"
#include "dolphin/os.h"

// Bytes left to the OS heap after MEM takes its block.
int iGC_OsMemory = 0x2000;
static int iGC_OsMemoryInitized;

u32 iGC_GetMaxOsMemory() {
    if (iGC_OsMemoryInitized == 0) {
        u32 lo = (u32)OSGetArenaLo();
        u32 hi = (u32)OSGetArenaHi();
        return ((hi & ~0x1F) - ((lo + 0x1F) & ~0x1F)) - iGC_OsMemory;
    }
    return 0;
}

// Turn the whole arena into the OS heap.
void iGC_InitMem() {
    if (iGC_OsMemoryInitized == 0) {
        void* lo;
        void* hi;
        iGC_OsMemoryInitized = 1;
        lo = OSGetArenaLo();
        hi = OSGetArenaHi();
        lo = OSInitAlloc(lo, hi, 1);
        OSSetArenaLo(lo);
        hi = (void*)((u32)hi & ~0x1F);
        OSSetCurrentHeap(OSCreateHeap((void*)(((u32)lo + 0x1F) & ~0x1F), hi));
        OSSetArenaLo(hi);
    }
}

// malloc/free of the MSL runtime.
extern "C" void* __sys_alloc(u32 size) {
    if (memclass[0] == NULL) {
        MEM_init();
    }
    return MEM_alloc("malloc pool", size, 0);
}

extern "C" void __sys_free(void* ptr) {
    if (memclass[0] == NULL) {
        MEM_init();
    }
    MEM_free(ptr);
}
