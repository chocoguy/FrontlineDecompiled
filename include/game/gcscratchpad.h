// The locked-cache scratch pad (game/gcscratchpad.cpp). Hand-maintained.
#ifndef _GAME_GCSCRATCHPAD_H_
#define _GAME_GCSCRATCHPAD_H_

#include "types.h"

// Its users keep pointers in it (A* queues, the scene's open-node list,
// compartment lights), so natively it is twice the size for the same counts.
#define SPR_SIZE GC_SIZE(0x4000, 0x8000)

extern u8 g_scratch_pad[SPR_SIZE];  // 0x801F30E0
extern u8* SPR_BEGIN;  // 0x8034E2C0
extern u8* SPR_END;  // 0x8034E2C4

#endif
