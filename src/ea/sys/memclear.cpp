#include "ea/sys/mem.h"

extern "C" void MEM_clear(void* dest, int size) {
    MEM_fill(dest, 0, size);
}
