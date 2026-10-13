// The scratch pad: a 16 KB buffer for short-lived work memory.
#include "game/gcscratchpad.h"

u8 g_scratch_pad[0x4000] __attribute__((aligned(32)));
u8* SPR_BEGIN = g_scratch_pad;
u8* SPR_END = g_scratch_pad + sizeof(g_scratch_pad);
