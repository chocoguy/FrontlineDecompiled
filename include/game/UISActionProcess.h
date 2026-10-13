// IStudio thread actions: deferred UI operations queued on a stack and run
// once per update. Hand-maintained.
#ifndef _GAME_UISACTIONPROCESS_H_
#define _GAME_UISACTIONPROCESS_H_

#include "types.h"
#include "game/uistypes.h"

void UISAddThreadAction(UISInfo_t* info, UISThreadActionT action, UISThreadGroupInfoT* group,
                        long numParams, UISParam_t* params);  // 0x80085678
// Runs and clears every queued action; with onlyControls, only applies the
// pending control (de)activations and keeps the queue.
void UISProcessThreadAction(UISInfo_t* info, unsigned char onlyControls);  // 0x800852C8

#endif
