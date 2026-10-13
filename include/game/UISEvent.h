// IStudio rate functions (animated control values). Hand-maintained.
#ifndef _GAME_UISEVENT_H_
#define _GAME_UISEVENT_H_

#include "types.h"
#include "game/uistypes.h"

// Index of the rate function for (control, id), or numRateFncs if none.
int UISFindRateFnc(UISInfo_t* info, UISControlInfo_t* control, unsigned long id);  // 0x80085A40
// Starts an animation of control2's value `action` to `target` over `ticks`.
void UISLoadAdvRateFnc(UISInfo_t* info, UISScreen_t* screen, UISControlInfo_t* control,
                       UISControlInfo_t* control2, unsigned long id, unsigned char* data2,
                       unsigned char* data, unsigned long ticks, float target,
                       unsigned long action);  // 0x800858FC
void UISLoadRateFnc(UISInfo_t* info, UISScreen_t* screen, UISControlInfo_t* control,
                    unsigned long id, unsigned char* data, unsigned long arg);  // 0x80085860
void UISUnloadRateFnc(UISInfo_t* info, UISControlInfo_t* control, unsigned long id);  // 0x800857FC
void UISRemoveUnNessaryRateFncs(UISInfo_t* info);  // 0x80085744

#endif
