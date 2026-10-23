#include "ea/sys/mem.h"
#include "ea/sys/mutex2.h"

// Grow or shrink a block in place (taking a free block after it); -1 asks for as much as fits.
// Returns the data pointer.
extern "C" void* MEM_resize(void* ptr, int size) {
    MEMBLOCK* block = MEM_header(ptr);
    int flags = block->flags;
    MEMCLASS* mc = memclass[flags & MB_CLASSMASK];
    MEMBLOCK* next;
    MEMBLOCK* end;
    int newSize;
    int tail;
    int mask;
    int avail;

    if (mc->hasMutex) {
        MUTEX_lock(mc->mutex);
    }
    next = block->next;
    if (next->flags & MB_FREE) {
        FREE_remove(next);
        next = next->next;
        block->next = next;
        next->prev = block;
    }

    newSize = size;
    if (size < 8) {
        if (size == -1) {
            newSize = 0x40000000;
        } else if (size >= 0) {
            newSize = 8;
        }
    }
    tail = MEM_tailsize(NULL, flags);
    avail = (u8*)next - (u8*)block - 0x10;
    mask = mc->blockAlign - 1;
    if (newSize + tail > avail) {
        newSize = avail - tail;
        size = newSize;
    }

    MEM_move((u8*)ptr + newSize, (u8*)ptr + block->size, tail);
    block->size = size;
    end = (MEMBLOCK*)(((uptr)(ptr) + newSize + tail + mask) & ~mask);
    if (((u8*)next - (u8*)end) / 0x10 > 0x30) {
        MEM_initblock(end, NULL, 0, 0, 0, block, next);
        FREE_add(mc, end);
        next->prev = end;
        block->next = end;
    }

    if (mc->hasMutex) {
        MUTEX_unlock(mc->mutex);
    }
    return ptr;
}
