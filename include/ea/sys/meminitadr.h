// Skeleton for ea/sys/meminitadr (from tools/gen_headers.py, now hand-maintained).
#ifndef _EA_SYS_MEMINITADR_H_
#define _EA_SYS_MEMINITADR_H_

#include "types.h"
#include "game_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Creates the main memory class over [addr, addr + size) via MEMCLASS_create
// and returns its result.
int MEM_initadr(void* addr, int size);  // 0x8014FB7C

#ifdef __cplusplus
}
#endif

#endif
