// Animation system setup and the game's handler for user opcodes in
// animation scripts. Hand-maintained.
#ifndef _GAME_ANIMINTF_H_
#define _GAME_ANIMINTF_H_

#include "types.h"

extern "C" {
void AnimSystem_Init(void);  // 0x80064530
void AnimSystem_Shutdown(void);  // 0x80064508
}

#endif
