#include "ea/sys/mem.h"

int MEM_initblock(MEMBLOCK* block, const char* name, int size, int tail, int flags, MEMBLOCK* prev,
                  MEMBLOCK* next) {
    block->magic = MB_MAGIC_NEW;
    block->flags = flags;
    block->size = size;
    block->next = next;
    block->prev = prev;
    return (u8*)block + (size + 0x10) - (u8*)block;
}
