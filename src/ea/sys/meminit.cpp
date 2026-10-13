#include "ea/sys/mem.h"
#include "ea/sys/meminitadr.h"
#include "ea/sys/exit.h"
#include "dolphin/os.h"

// Make class 0 from an OS heap block as large as the OS can spare.
extern "C" int MEM_init(void) {
    int result = 0;
    u32 size = iGC_GetMaxOsMemory();
    void* block;

    REAL_addexit(MEM_restore);
    iGC_InitMem();
    block = OSAllocFromHeap(__OSCurrHeap, size);
    if (block != NULL) {
        result = MEM_initadr(block, size);
        memclass[0]->osBlock = block;
    }
    return result;
}
