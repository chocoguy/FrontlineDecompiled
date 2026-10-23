// EA block pools (BPoolMan): fixed-size elements handed out from a FIFO free list.
#ifndef _EA_SYS_BMEM_H_
#define _EA_SYS_BMEM_H_

#include "types.h"

// The links are 32-bit natively too: elements can be 4 bytes, and the
// headers' sizes are fixed (0x20).
struct BPoolNode {
    PTR32(BPoolNode) next;
};

// A pool of elements; the elements start 0x20 bytes in.
struct BPool {
    /* 0x0 */ PTR32(BPool) next;  // circular, through the manager
    /* 0x4 */ u32 count;
    /* 0x8 */ u8 unk_0x8[0x18];
};

struct BPoolMan {
    /* 0x00 */ PTR32(BPool) firstPool;
    /* 0x04 */ PTR32(BPool) lastPool;
    /* 0x08 */ BPoolNode freeHead;  // free elements, circular through freeHead
    /* 0x0C */ PTR32(BPoolNode) freeLast;
    /* 0x10 */ u32 elemSize;
    /* 0x14 */ u8 unk_0x14[0xC];
    // first pool at 0x20
};
FILE_SIZE_ASSERT(BPoolNode, 0x4);
FILE_SIZE_ASSERT(BPool, 0x20);
FILE_SIZE_ASSERT(BPoolMan, 0x20);

// Take the first free element. The list counts as empty while one element is left.
static inline void* BAlloc(BPoolMan* man) {
    BPoolNode* head = &man->freeHead;
    BPoolNode* node;
    if (head->next == man->freeLast) {
        node = NULL;
    } else {
        if (head->next == man->freeLast) {
            man->freeLast = head;
        }
        node = head->next;
        head->next = node->next;
    }
    return node;
}

extern "C" {
BPoolMan* NEW_BPoolMan(u32 count, u32 size);  // 0x8014FF44
void CON_BPoolMan(BPoolMan* man, u32 count, u32 size);  // 0x8014FFA4
void BFree(BPoolMan* man, void* elem);  // 0x8015000C
}

#endif
