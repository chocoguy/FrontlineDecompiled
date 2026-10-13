#include "ea/sys/bmem.h"
#include "ea/sys/mem.h"

static void AddNewPool(BPoolMan* man, BPool* pool, unsigned int count);

extern "C" BPoolMan* NEW_BPoolMan(u32 count, u32 size) {
    BPoolMan* man = (BPoolMan*)MEM_alloc("BPoolMan", count * size + 0x60, mb_default);
    CON_BPoolMan(man, count, size);
    return man;
}

extern "C" void CON_BPoolMan(BPoolMan* man, u32 count, u32 size) {
    u32 elemSize = 4;
    if (size > 4) {
        elemSize = size;
    }
    elemSize = (elemSize + 3) & ~3;
    u32 n = 1;
    if (count > 1) {
        n = count;
    }
    man->firstPool = (BPool*)man;
    man->lastPool = (BPool*)man;
    man->freeHead.next = &man->freeHead;
    man->freeLast = &man->freeHead;
    man->elemSize = elemSize;
    AddNewPool(man, (BPool*)((u8*)man + 0x20), n);
}

extern "C" void BFree(BPoolMan* man, void* elem) {
    BPoolNode* node = (BPoolNode*)elem;
    man->freeLast->next = node;
    man->freeLast = node;
    node->next = &man->freeHead;
}

#pragma auto_inline off
static void AddNewPool(BPoolMan* man, BPool* pool, unsigned int count) {
    u32 i;
    pool->count = count;
    man->lastPool->next = pool;
    man->lastPool = pool;
    pool->next = (BPool*)man;
    for (i = 0; i < count; i++) {
        BPoolNode* node = (BPoolNode*)((u8*)pool + i * man->elemSize + 0x20);
        man->freeLast->next = node;
        man->freeLast = node;
        node->next = &man->freeHead;
    }
}
#pragma auto_inline reset
