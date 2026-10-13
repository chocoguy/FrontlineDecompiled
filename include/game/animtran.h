// Animation state transitions: picks how to blend from the current state's
// animation into a new state's (cross-fade, cut, or a clip in between).
// Hand-maintained.
#ifndef _GAME_ANIMTRAN_H_
#define _GAME_ANIMTRAN_H_

#include "types.h"

struct AnimChannel_t;
struct AnimObjHdr_t;
struct AnimStInfo_t;
struct AnimTranTransListArray_t;

// AnimTransition_t::type
#define ANIMTRAN_CUT 0
#define ANIMTRAN_BLEND 1
#define ANIMTRAN_CUT2 2
#define ANIMTRAN_PLAY 3

// A state in 0x37AA..0x37DB that leads from `from` into `state` when there
// is no direct transition, or -1 (callers test for a negative result).
int _AnimTranQuickScan(AnimObjHdr_t* hdr, u16 from, u16 anim, u16 state, u16 cond);  // 0x800681B0
// Starts the transitions of the slots whose status is `status` into `state`.
void _AnimTranDoTransition(AnimStInfo_t* stateInfo, AnimObjHdr_t* hdr, AnimChannel_t* chans,
                           u16 state, u16 listState, unsigned long status,
                           void* userData);  // 0x8006862C

extern AnimTranTransListArray_t AnimTran_TransListArray;  // 0x801D49F8

#endif
