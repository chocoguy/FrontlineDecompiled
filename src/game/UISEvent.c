#include "game/UISEvent.h"

#include <string.h>
#include "game/UISUtils.h"


int UISFindRateFnc(UISInfo_t* info, UISControlInfo_t* control, unsigned long id) {
    u32 i;
    for (i = 0; i < info->numRateFncs; i++) {
        UISRateFnc_t* fnc = &info->rateFncs[i];
        if (fnc->id == id && fnc->control == control) {
            break;
        }
    }
    return i;
}

static inline UISRateFnc_t* GetRateFncSlot(UISInfo_t* info, UISControlInfo_t* control,
                                           unsigned long id) {
    u32 i = UISFindRateFnc(info, control, id);
    if (i == info->numRateFncs) {
        info->numRateFncs++;
    }
    return &info->rateFncs[i];
}

void UISLoadAdvRateFnc(UISInfo_t* info, UISScreen_t* screen, UISControlInfo_t* control,
                       UISControlInfo_t* control2, unsigned long id, unsigned char* data2,
                       unsigned char* data, unsigned long ticks, float target,
                       unsigned long action) {
    UISRateFnc_t* fnc = GetRateFncSlot(info, control, id);
    fnc->control = control;
    fnc->control2 = control2;
    fnc->screen = screen;
    fnc->id = id;
    fnc->data = data;
    fnc->interval = info->ticksPerSecond;
    fnc->ticks = 0;
    fnc->ticksDone = 0;
    fnc->state = RATEFNC_NEW;
    fnc->action = action;
    fnc->target = target;
    fnc->data2 = data2;
    float current = *UISGetActionPtrValue(fnc->action, fnc->control2);
    fnc->rate = (target - current) / ((float)ticks / (float)info->ticksPerSecond);
}

void UISLoadRateFnc(UISInfo_t* info, UISScreen_t* screen, UISControlInfo_t* control,
                    unsigned long id, unsigned char* data, unsigned long arg) {
    UISRateFnc_t* fnc = GetRateFncSlot(info, control, id);
    fnc->control = control;
    fnc->id = id;
    fnc->data = data;
    fnc->interval = arg;
    fnc->ticks = 0;
    fnc->ticksDone = 0;
    fnc->screen = screen;
    fnc->state = RATEFNC_NEW;
    fnc->action = 0;
    fnc->target = 0.0f;
    fnc->data2 = NULL;
}

void UISUnloadRateFnc(UISInfo_t* info, UISControlInfo_t* control, unsigned long id) {
    u32 i = UISFindRateFnc(info, control, id);
    if (i < info->numRateFncs) {
        info->rateFncs[i].state = RATEFNC_FINISHED;
    }
}

// Deletes finished rate functions and marks the running ones.
void UISRemoveUnNessaryRateFncs(UISInfo_t* info) {
    int i = info->numRateFncs;
    while (i-- != 0) {
        u32 state = info->rateFncs[i].state;
        if (state == RATEFNC_FINISHED) {
            info->numRateFncs--;
            for (int j = i; j < (int)info->numRateFncs; j++) {
                memmove(&info->rateFncs[j], &info->rateFncs[j + 1], sizeof(UISRateFnc_t));
            }
        } else if (state == RATEFNC_NEW) {
            info->rateFncs[i].state = RATEFNC_RUNNING;
        }
    }
}
