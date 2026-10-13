// Skeleton for game/memory (from tools/gen_headers.py, now hand-maintained).
// Game allocators on top of EA's MEM library. All allocations are zeroed.
#ifndef _GAME_MEMORY_H_
#define _GAME_MEMORY_H_

#include "types.h"
#include "game_types.h"

void* DWI_allocalign(const char* name, int size, int align, int flags);  // 0x8001C8EC
void* DWI_alloc(const char* name, int size, int flags);                  // 0x8001C974
void operator delete[](void* ptr);                   // 0x8001C9E8
void operator delete(void* ptr);                     // 0x8001CA2C
void* operator new[](unsigned long size, char* name); // 0x8001CA70
void* operator new[](unsigned long size);            // 0x8001CAE8
void* operator new(unsigned long size);              // 0x8001CB60
inline void* operator new(unsigned long, void* where) { return where; }  // placement (inlined)

#endif
