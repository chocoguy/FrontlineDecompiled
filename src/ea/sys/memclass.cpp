#include "ea/sys/mem.h"
#include "ea/sys/mutex2.h"
#include <stdio.h>
#include <string.h>

static const int MB_SENTINEL = 0x2000;
static const int MB_NAME = 0x1000;

// Lay out a class over [base, base + size): a LOW block holding the MEMCLASS, one free block,
// and a HIGH block at the end. Returns the free space.
extern "C" int MEMCLASS_create(int id, const char* name, void* base, int size, int blockAlign,
                               int dataAlign, int tailSize, bool sentinel, bool named, bool mutex) {
    char label[256];
    int flags = id;
    MEMBLOCK* low = (MEMBLOCK*)base;
    MEMBLOCK* mid;
    MEMBLOCK* high;
    MEMCLASS* mc;

    if (sentinel) {
        flags |= MB_SENTINEL;
    }
    if (named) {
        flags |= MB_NAME;
    }
    mid = (MEMBLOCK*)((((uptr)(base) + (tailSize + 0x9C) + dataAlign - 1) & ~(dataAlign - 1)) - 0x10);
    high = (MEMBLOCK*)((u8*)base + (size - 0x30) - tailSize);
    mc = (MEMCLASS*)MEM_data(low);

    sprintf(label, "%s LOW", name);
    MEM_initblock(low, label, sizeof(MEMCLASS), tailSize, flags | MB_FIXED, NULL, mid);
    MEM_initblock(mid, NULL, (u8*)high - (u8*)mid - 0x10, tailSize, flags, low, high);
    sprintf(label, "%s HIGH", name);
    MEM_initblock(high, label, 0, tailSize, flags | MB_FIXED | MB_TOP, mid, NULL);

    memclass[id & MB_CLASSMASK] = mc;
    MEM_fill(mc, 0, sizeof(MEMCLASS));
    strcpy(mc->name, name);
    mc->low = low;
    mc->high = high;
    mc->low->magic = MB_MAGIC_SENTINEL;
    mc->high->magic = MB_MAGIC_SENTINEL;
    mc->freeList.magic = MB_MAGIC_SENTINEL;
    mc->freeList.nextFree = &mc->freeList;
    mc->freeList.prevFree = &mc->freeList;
    mc->freeList.size = 0x7FFFFFFF;
    mc->blockAlign = blockAlign;
    mc->dataAlign = dataAlign;
    mc->tailSize = tailSize;
    mc->flags = flags;
    mc->hasMutex = 0;
    FREE_add(mc, mid);
    if (mutex) {
        MUTEX_create(mc->mutex);
        mc->hasMutex = 1;
    }
    return mid->size;
}

extern "C" int MEMCLASS_remove(int id) {
    MEMCLASS* mc = memclass[id & MB_CLASSMASK];
    int removed = 0;
    if (mc != NULL) {
        if (mc->hasMutex) {
            MUTEX_destroy(mc->mutex);
        }
        MEM_fill(mc, 0, sizeof(MEMCLASS));
        removed = 1;
        memclass[id & MB_CLASSMASK] = NULL;
    }
    return removed;
}
