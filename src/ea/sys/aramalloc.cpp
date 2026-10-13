#include "ea/sys/aramalloc.h"
#include "ea/sys/mem.h"
#include "ea/sys/printstr.h"
#include <string.h>

extern "C" ARAM_POOL_MANAGER* ARAM_NEW_poolmanager(u32 align, u32 maxAllocs, u32 unused, const char* name) {
    ARAM_POOL_MANAGER* man = (ARAM_POOL_MANAGER*)MEM_alloc(name, sizeof(ARAM_POOL_MANAGER), mb_default);
    man->first = (ARAM_POOL*)man;
    man->last = (ARAM_POOL*)man;
    man->align = align;
    man->nodes = NEW_BPoolMan(maxAllocs, sizeof(ARAM_NODE));
    man->unk_0xC = 0;
    return man;
}

static inline void ARAM_append(ARAM_LINK* list, ARAM_LINK* link) {
    ARAM_LINK* last = list->prev;
    list->prev = link;
    last->next = link;
    link->next = list;
    link->prev = last;
}

static inline void ARAM_unlink(ARAM_LINK* link) {
    ARAM_LINK* next = link->next;
    ARAM_LINK* prev = link->prev;
    prev->next = next;
    next->prev = prev;
}

extern "C" ARAM_POOL* ARAM_NEW_pool(ARAM_POOL_MANAGER* man, u32 start, u32 size, const char* name) {
    ARAM_POOL* pool = (ARAM_POOL*)MEM_alloc(name, sizeof(ARAM_POOL), mb_default);
    ARAM_POOL* last;
    ARAM_NODE* node;

    pool->name = (char*)MEM_alloc("ARAM Pool Name", 0x1C, mb_default);
    strcpy(pool->name, name);
    if (start & (man->align - 1)) {
        PRINT_string(PRINTCHANNEL_2, "ARAMStartAddress is not Aligned\n");
        start = (start + man->align - 1) & ~(man->align - 1);
    }
    if (size & (man->align - 1)) {
        PRINT_string(PRINTCHANNEL_2, "ARAMMemorySize is not Aligned\n");
        size &= ~(man->align - 1);
    }

    last = man->last;
    man->last = pool;
    last->next = pool;
    pool->next = (ARAM_POOL*)man;
    pool->prev = last;
    pool->freeList.next = &pool->freeList;
    pool->freeList.prev = &pool->freeList;
    pool->usedList.next = &pool->usedList;
    pool->usedList.prev = &pool->usedList;

    node = (ARAM_NODE*)BAlloc(man->nodes);
    node->addr = start;
    node->size = size;
    ARAM_append(&pool->freeList, &node->link);
    return pool;
}

// Take `size` bytes from the first fitting free range of any pool. A sentinel node of size
// 0xFFFFFFFF on the stack ends each search.
#pragma auto_inline off
static ARAM_NODE* ARAM_alloc_node(ARAM_POOL_MANAGER* man, const char* name, unsigned int size, ARAM_DIRECTION direction) {
    ARAM_NODE* result = NULL;
    ARAM_POOL* pool;
    u32 addr;

    size = (size + man->align - 1) & ~(man->align - 1);
    if (direction == ARAM_BOTTOM) {
        for (pool = man->first; pool != (ARAM_POOL*)man; pool = pool->next) {
            ARAM_NODE sentinel;
            ARAM_NODE* node;
            ARAM_NODE* range;

            sentinel.size = 0xFFFFFFFF;
            ARAM_append(&pool->freeList, &sentinel.link);
            for (range = (ARAM_NODE*)pool->freeList.next; range->size < size; range = (ARAM_NODE*)range->link.next) {
            }
            ARAM_unlink(&sentinel.link);
            if (range == &sentinel) {
                node = NULL;
            } else {
                if (range->size > size) {
                    addr = range->addr;
                    range->addr = addr + size;
                    range->size -= size;
                } else {
                    addr = range->addr;
                    ARAM_unlink(&range->link);
                    BFree(man->nodes, range);
                }
                node = (ARAM_NODE*)BAlloc(man->nodes);
                if (node == NULL) {
                    PRINT_string(PRINTCHANNEL_2,
                                 "Out Of ARAM malloc nodes add to your MaxNumberOfMallocs with ARAM_NEW_poolmanager\n");
                    node = NULL;
                } else {
                    node->size = size;
                    node->addr = addr;
                    node->pool = pool;
                    ARAM_append(&pool->usedList, &node->link);
                }
            }
            result = node;
            if (node != NULL) {
                return result;
            }
        }
    } else if (direction == ARAM_TOP) {
        for (pool = man->first; pool != (ARAM_POOL*)man; pool = pool->next) {
            ARAM_NODE sentinel;
            ARAM_NODE* node;
            ARAM_NODE* range;
            ARAM_LINK* first;

            sentinel.size = 0xFFFFFFFF;
            first = pool->freeList.next;
            pool->freeList.next = &sentinel.link;
            first->prev = &sentinel.link;
            sentinel.link.prev = &pool->freeList;
            sentinel.link.next = first;
            for (range = (ARAM_NODE*)pool->freeList.prev; range->size < size; range = (ARAM_NODE*)range->link.prev) {
            }
            ARAM_unlink(&sentinel.link);
            if (range == &sentinel) {
                node = NULL;
            } else {
                if (range->size > size) {
                    range->size -= size;
                    addr = range->addr + range->size;
                } else {
                    addr = range->addr;
                    ARAM_unlink(&range->link);
                    BFree(man->nodes, range);
                }
                node = (ARAM_NODE*)BAlloc(man->nodes);
                if (node == NULL) {
                    PRINT_string(PRINTCHANNEL_2,
                                 "Out Of ARAM malloc nodes add to your MaxNumberOfMallocs with ARAM_NEW_poolmanager\n");
                    node = NULL;
                } else {
                    node->size = size;
                    node->addr = addr;
                    node->pool = pool;
                    ARAM_append(&pool->usedList, &node->link);
                }
            }
            result = node;
            if (node != NULL) {
                return result;
            }
        }
    }
    return result;
}

#pragma auto_inline reset

extern "C" u32 ARAM_alloc(ARAM_POOL_MANAGER* man, const char* name, u32 size, ARAM_DIRECTION direction) {
    ARAM_NODE* node = ARAM_alloc_node(man, name, size, direction);
    if (node == NULL) {
        PRINT_string(PRINTCHANNEL_2, "\nOut Of ARAM!!\n PoolManager 0x%p Size %d Type %d\n DebugString %s", man,
                     size, direction, name);
        return 0;
    }
    return node->addr;
}
