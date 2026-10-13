// BS script timers: delayed events kept in a 120-bucket wheel keyed by the
// whole-second fire time. Hand-maintained.
#ifndef _GAME_BSTIMER_H_
#define _GAME_BSTIMER_H_

#include "types.h"
#include "game/bstypes.h"

int BSTimerGetMemoryRequirements();  // 0x8003B544
void BSInitTimer();  // 0x8003B2E8
void BSEndTimer();  // 0x8003B2D4
void BSUpdateTimer(float dt);  // 0x8003B18C
// Raises `event` on `obj` after `delay` seconds.
void BSRegisterTimerEvent(int delay, unsigned short event, BSObject* obj, void* data,
                          ETimerReplaceMethod method);  // 0x8003AE40
void BSUnregisterTimerEvents(BSObject* obj);  // 0x8003AD68

#endif
