#include "ea/sys/mem.h"

extern "C" int MEM_totalunused(int flags) {
    if (flags == 0) {
        flags = mb_default;
    }
    return FREE_gettotalfree(memclass[flags & MB_CLASSMASK], 0);
}
