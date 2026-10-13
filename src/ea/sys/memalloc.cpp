#include "ea/sys/mem.h"
#include "ea/sys/mutex2.h"

MEM_AllocFailFn MEM_allocfailcallback;

int MEM_tailsize(const char* name, int flags) {
    return memclass[flags & MB_CLASSMASK]->tailSize;
}

// Find a free block big enough for `size` bytes with data + offset aligned, carve the block out
// of it and give the rest back to the free list when it is worth keeping.
static void* MEM_allocaligna(const char* name, int size, int align, int offset, int flags, bool) {
    void* data = NULL;
    MEMCLASS* mc;
    int classAlign;
    int largest;
    int fromTop;
    int retry;

    if (flags == 0) {
        flags = mb_default;
    }
    mc = memclass[flags & MB_CLASSMASK];
    if (size < 0) {
        return NULL;
    }
    classAlign = flags & MB_CLASSALIGN;
    largest = flags & MB_LARGEST;
    fromTop = flags & MB_TOP;

    do {
        int blockMask;
        int alignMask;
        int a;
        int need;
        int total;
        MEMBLOCK* block;

        retry = 0;
        if (mc->hasMutex) {
            MUTEX_lock(mc->mutex);
        }
        a = mc->blockAlign;
        blockMask = a - 1;
        if (align == 0) {
            if (classAlign) {
                a = mc->dataAlign;
            }
            alignMask = a - 1;
        } else {
            alignMask = align - 1;
        }
        need = 8;
        if (size + memclass[flags & MB_CLASSMASK]->tailSize >= 8) {
            need = size + memclass[flags & MB_CLASSMASK]->tailSize;
        }
        total = need + alignMask + 0x10;

        if (largest) {
            block = FREE_findlargest(mc, total, fromTop);
        } else {
            block = FREE_find(mc, total, fromTop);
        }

        if (block != NULL) {
            int blockSize = block->size;
            MEMBLOCK* rest = NULL;
            MEMBLOCK* next;
            MEMBLOCK* prev;
            MEMBLOCK* header;

            FREE_remove(block);
            next = block->next;
            prev = block->prev;
            if (fromTop) {
                rest = block;
                data = (void*)((((u32)block + blockSize - need + offset) & ~alignMask) - offset);
            } else {
                data = (void*)((((u32)block + alignMask + offset + 0x10) & ~alignMask) - offset);
            }
            header = MEM_header(data);
            header->next = next;
            header->prev = prev;

            if (blockSize - total > blockMask + 0x30) {
                if (fromTop) {
                    // The free remainder stays below the new block.
                    rest->next->prev = header;
                    header->next = rest->next;
                    header->prev = rest;
                    MEM_initblock(rest, NULL, 0, 0, 0, rest->prev, header);
                    FREE_add(mc, rest);
                } else {
                    // The free remainder starts after the new block's data.
                    rest = (MEMBLOCK*)(((u32)data + need + blockMask) & ~blockMask);
                    header->next->prev = rest;
                    header->prev->next = header;
                    MEM_initblock(rest, NULL, 0, 0, 0, header, header->next);
                    FREE_add(mc, rest);
                    header->next = rest;
                }
            } else {
                header->next->prev = header;
                header->prev->next = header;
            }
            MEM_initblock(header, NULL, size, mc->tailSize, flags, header->prev, header->next);
        } else if (MEM_allocfailcallback != NULL) {
            retry = MEM_allocfailcallback(NULL, size, flags);
        }

        if (mc->hasMutex) {
            MUTEX_unlock(mc->mutex);
        }
    } while (retry == 1);
    return data;
}

extern "C" void* MEM_allocalign(const char* name, u32 size, s32 align, s32 offset, s32 flags) {
    return MEM_allocaligna(name, size, align, offset, flags, true);
}

extern "C" void* MEM_alloc(const char* name, u32 size, s32 flags) {
    return MEM_allocaligna(name, size, 0, 0, flags, true);
}

extern "C" void* MEM_allocz(const char* name, int size, int flags) {
    return MEM_allocaligna(name, size, 0, 0, flags, false);
}
