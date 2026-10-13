#include "ea/sys/mem.h"

static const int MB_FREE_FLAG = MB_FREE;

// First free block of at least `size` bytes, searching up or down the address list.
MEMBLOCK* FREE_find(MEMCLASS* mc, int size, int fromTop) {
    MEMBLOCK* block = &mc->freeList;
    if (fromTop == 0) {
        do {
            block = block->nextFree;
        } while (size > block->size);
    } else {
        do {
            block = block->prevFree;
        } while (size > block->size);
    }
    if (block->magic == MB_MAGIC_SENTINEL) {
        return NULL;
    }
    return block;
}

// Largest free block with more than `size - 1` usable bytes.
MEMBLOCK* FREE_findlargest(MEMCLASS* mc, int size, int fromTop) {
    MEMBLOCK* block = &mc->freeList;
    MEMBLOCK* best = NULL;
    int bestSize = size - 1;
    int blockSize;
    if (bestSize < 0) {
        bestSize = 0;
    }
    if (fromTop == 0) {
        for (;;) {
            block = block->nextFree;
            blockSize = block->size - 0x10;
            if (blockSize > bestSize) {
                if (block->magic == MB_MAGIC_SENTINEL) {
                    return best;
                }
                best = block;
                bestSize = blockSize;
            }
        }
    } else {
        for (;;) {
            block = block->prevFree;
            blockSize = block->size - 0x10;
            if (blockSize > bestSize) {
                if (block->magic == MB_MAGIC_SENTINEL) {
                    return best;
                }
                best = block;
                bestSize = blockSize;
            }
        }
    }
    return best;
}

int FREE_gettotalfree(MEMCLASS* mc, int fromTop) {
    MEMBLOCK* block = &mc->freeList;
    int total = 0;
    if (fromTop == 0) {
        for (;;) {
            block = block->nextFree;
            if (block->magic == MB_MAGIC_SENTINEL) {
                return total;
            }
            total += block->size - 0x10;
        }
    } else {
        for (;;) {
            block = block->prevFree;
            if (block->magic == MB_MAGIC_SENTINEL) {
                return total;
            }
            total += block->size - 0x10;
        }
    }
    return total;
}

// Insert in address order, walking from whichever end is nearer.
void FREE_add(MEMCLASS* mc, MEMBLOCK* block) {
    MEMBLOCK* next = &mc->freeList;
    MEMBLOCK* prev = next;
    int span = (u8*)block->next - (u8*)block;

    if ((u8*)block > (u8*)next->nextFree + ((u8*)next->prevFree - (u8*)next->nextFree) / 2) {
        do {
            prev = prev->prevFree;
        } while (block < prev);
        next = prev->nextFree;
    } else {
        do {
            next = next->nextFree;
        } while (block > next);
        prev = next->prevFree;
    }
    block->nextFree = next;
    block->prevFree = prev;
    block->size = span;
    prev->nextFree = block;
    next->prevFree = block;
    block->flags |= MB_FREE_FLAG;
    block->magic = MB_MAGIC_FREE;
}

void FREE_remove(MEMBLOCK* block) {
    MEMBLOCK* prev = block->prevFree;
    MEMBLOCK* next = block->nextFree;
    prev->nextFree = next;
    next->prevFree = prev;
    block->flags &= ~MB_FREE_FLAG;
    block->magic = MB_MAGIC_USED;
}
