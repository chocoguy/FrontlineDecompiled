// Skeleton for game/real_bridge (from tools/gen_headers.py, now hand-maintained).
// Starts and stops EA's REAL runtime library.
#ifndef _GAME_REAL_BRIDGE_H_
#define _GAME_REAL_BRIDGE_H_

#include "types.h"
#include "game_types.h"

void ShutdownREAL();      // 0x8001B068
void UpdateREAL();        // 0x8001B098
void InitREAL(int arg);   // 0x8001B0BC

#endif
