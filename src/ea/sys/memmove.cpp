#include "ea/sys/mem.h"

// Copy that allows overlap: backwards when dest lies inside the source.
extern "C" void MEM_move(void* dest, const void* src, int size) {
    if (dest <= src || dest >= (const u8*)src + size) {
        MEM_copy(dest, src, size);
    } else {
        u8* d = (u8*)dest + size;
        const u8* s = (const u8*)src + size;
        while (size != 0) {
            *--d = *--s;
            size--;
        }
    }
}
