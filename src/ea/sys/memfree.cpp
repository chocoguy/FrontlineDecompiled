#include "ea/sys/mem.h"
#include "ea/sys/mutex2.h"

// Merge with free neighbours; if the block before is in use, the free block starts right
// after that block's data (taking back alignment padding).
extern "C" int MEM_free(void* ptr) {
    if (ptr != NULL) {
        MEMBLOCK* block = MEM_header(ptr);
        MEMCLASS* mc = memclass[block->flags & MB_CLASSMASK];
        MEMBLOCK* prev;
        MEMBLOCK* next;

        if (mc->hasMutex) {
            MUTEX_lock(mc->mutex);
        }
        prev = block->prev;
        next = block->next;
        if (prev->flags & MB_FREE) {
            FREE_remove(prev);
            block = prev;
            prev = prev->prev;
            block->next = next;
            prev->next = block;
            next->prev = block;
        }
        if (next->flags & MB_FREE) {
            FREE_remove(next);
            next = next->next;
            block->prev = prev;
            block->next = next;
            next->prev = block;
        }
        if ((prev->flags & (MB_FREE | MB_FIXED)) == 0) {
            int used = prev->size + MEM_tailsize(NULL, prev->flags);
            int mask;
            MEMBLOCK* start;
            if (used < 8) {
                used = 8;
            }
            mask = mc->blockAlign - 1;
            start = (MEMBLOCK*)((u8*)prev + ((used + 0x10 + mask) & ~mask));
            if (block != start) {
                u16 flags = block->flags;
                block = start;
                start->flags = flags;
                start->next = next;
                start->prev = prev;
                prev->next = start;
                next->prev = start;
            }
        }
        FREE_add(mc, block);
        if (mc->hasMutex) {
            MUTEX_unlock(mc->mutex);
        }
    }
    return 1;
}
