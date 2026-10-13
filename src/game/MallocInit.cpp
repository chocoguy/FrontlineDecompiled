#include "game/MallocInit.h"

#include "dolphin/os.h"
#include "ea/sys/meminitadr.h"

bool g_bMallocInitialized;

void mallocinit(void) {
    u8* lo = (u8*)OSGetArenaLo();
    s32 size = (u8*)OSGetArenaHi() - lo;

    // Arenas over 24 MB (e.g. 48 MB development hardware) are reduced by 24 MB.
    if (size > 0x1800000) {
        size -= 0x1800000;
    }

    int result = MEM_initadr(lo, size);
    g_bMallocInitialized = true;

    OSReport("\n");
    OSReport("\n");
    OSReport("MEM_init() return %d\n", result);
    OSReport("\n");
}
