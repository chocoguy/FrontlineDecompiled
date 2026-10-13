// Skeleton for game/MallocInit (from tools/gen_headers.py, now hand-maintained).
#ifndef _GAME_MALLOCINIT_H_
#define _GAME_MALLOCINIT_H_

#include "types.h"
#include "game_types.h"

// Set once mallocinit() has run; the allocators in memory.cpp check it and
// initialize the heap lazily on first use.
extern bool g_bMallocInitialized;

#ifdef __cplusplus
extern "C" {
#endif

// Hands the OS arena to the EA memory system.
void mallocinit(void);  // 0x8001C85C

#ifdef __cplusplus
}
#endif

#endif
