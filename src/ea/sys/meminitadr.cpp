#include "ea/sys/mem.h"
#include "ea/sys/meminitadr.h"

extern "C" int MEM_initadr(void* addr, int size) {
    return MEMCLASS_create(0, "RAM", addr, size, 0x20, 0x20, 0, false, false, true);
}
