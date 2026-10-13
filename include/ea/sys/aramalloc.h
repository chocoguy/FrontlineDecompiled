// EA ARAM allocator: pools of ARAM address ranges with free/used node lists.
#ifndef _EA_SYS_ARAMALLOC_H_
#define _EA_SYS_ARAMALLOC_H_

#include "types.h"
#include "ea/sys/bmem.h"

struct ARAM_LINK {
    ARAM_LINK* next;
    ARAM_LINK* prev;
};

struct ARAM_POOL;

// A range of ARAM, free or allocated (elements of the manager's node pool).
struct ARAM_NODE {
    /* 0x00 */ ARAM_LINK link;
    /* 0x08 */ u32 addr;
    /* 0x0C */ u32 size;
    /* 0x10 */ ARAM_POOL* pool;
    /* 0x14 */ u8 unk_0x14[0x8];
};

struct ARAM_POOL {
    /* 0x00 */ ARAM_POOL* next;  // circular, through the manager
    /* 0x04 */ ARAM_POOL* prev;
    /* 0x08 */ ARAM_LINK freeList;  // address order
    /* 0x10 */ ARAM_LINK usedList;
    /* 0x18 */ char* name;
};

struct ARAM_POOL_MANAGER {
    /* 0x00 */ ARAM_POOL* first;
    /* 0x04 */ ARAM_POOL* last;
    /* 0x08 */ u32 align;
    /* 0x0C */ u32 unk_0xC;
    /* 0x10 */ BPoolMan* nodes;
};

// Allocation direction: from the bottom or the top of a pool's free ranges.
enum ARAM_DIRECTION {
    ARAM_BOTTOM = 0,
    ARAM_TOP = 0x100,
};

extern "C" {
ARAM_POOL_MANAGER* ARAM_NEW_poolmanager(u32 align, u32 maxAllocs, u32 unused, const char* name);  // 0x801501EC
ARAM_POOL* ARAM_NEW_pool(ARAM_POOL_MANAGER* manager, u32 start, u32 size, const char* name);  // 0x80150260
u32 ARAM_alloc(ARAM_POOL_MANAGER* manager, const char* name, u32 size, ARAM_DIRECTION direction);  // 0x8015067C
}

#endif
