#include "game/UISActionProcess.h"

#include "game/UISUtils.h"
#include "game/UIStudio.h"


// Push a record; the stack grows downward. The parameters are only reserved
// when `params` is given.
static inline void PushThreadAction(UISInfo_t* info, u32 action, UISThreadGroupInfoT* group,
                                    long numParams, UISParam_t* params) {
    u32* top = info->actionStackPtr;
    UISThreadActionRecord* rec = (UISThreadActionRecord*)(top - 6);
    u32* dst = top - 7;
    rec->action = action;
    rec->group = *group;
    rec->numParams = numParams;
    if (params != NULL) {
        u32* src = &params[numParams - 1].value;
        for (long i = numParams - 1; i >= 0; i--) {
            *dst-- = *src--;
        }
    }
    info->actionStackPtr = dst;
}

void UISAddThreadAction(UISInfo_t* info, UISThreadActionT action, UISThreadGroupInfoT* group,
                        long numParams, UISParam_t* params) {
    PushThreadAction(info, action, group, numParams, params);
}

static inline UISThreadActionRecord* RecordAt(u32* top) {
    return (UISThreadActionRecord*)(top - 6);
}

static inline UISParam_t* RecordParams(u32* top) {
    return (UISParam_t*)(top - 6 - RecordAt(top)->numParams);
}

static inline void ActivateControl(UISInfo_t* info, u8 activate, UISThreadGroupInfoT* g) {
    if (g->sh[1] == 0) {
        UISInternalActivateControl(info, activate, g->sh[0], ADDR_TO_PTR(UISControlInfo_t, g->w[2]),
                                   ADDR_TO_PTR(UISControlList_t, g->w[1]), g->h[6], g->h[7]);
        g->sh[1] = 1;
    }
}

// Runs the record whose top is `topParam`; returns the next record's top.
static UISParam_t* _UISDoThreadAction(UISInfo_t* info, UISParam_t* topParam) {
    u32* top = &topParam->value;
    UISThreadActionRecord* rec = RecordAt(top);
    UISThreadGroupInfoT* g = &rec->group;
    u32 action = rec->action;
    u32 numParams = rec->numParams;
    UISParam_t* params = RecordParams(top);

    switch (action) {
    case UIS_TA_LOAD_SCREEN:
        UISInternalLoadScreen(info, g->h[0], g->h[1], g->h[2], g->h[3], numParams, params);
        break;
    case UIS_TA_UNLOAD_SCREEN:
        // Retried next update until the screen can be unloaded.
        if (!UISInternalUnloadScreen(info, g->h[0], g->h[1], g->w[2])) {
            PushThreadAction(info, action, g, numParams, params);
        }
        break;
    case UIS_TA_HINT:
        UISDoHint(info, g->w[0], numParams, params);
        break;
    case UIS_TA_SEND_EVENT:
        info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
        UISProcessInternalEvents(info, &info->stack, g->w[0], (unsigned long)-8, numParams, params,
                                 1);
        info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
        break;
    case UIS_TA_ACTIVATE_SCREEN:
        UISInternalActivateScreen(info, 1, g->h[0], g->h[1]);
        break;
    case UIS_TA_DEACTIVATE_SCREEN:
        UISInternalActivateScreen(info, 0, g->h[0], g->h[1]);
        break;
    case UIS_TA_ACTIVATE_CONTROL:
        ActivateControl(info, 1, g);
        break;
    case UIS_TA_DEACTIVATE_CONTROL:
        ActivateControl(info, 0, g);
        break;
    case UIS_TA_SEND_EVENT_TO:
        info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
        UISProcessInternalEvents(info, &info->stack, g->w[1], g->w[0], numParams, params, 0);
        info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
        break;
    case UIS_TA_MOVE_SCREEN:
        UISMoveScreenDrawPosition(info, g->h[0], g->h[1], g->w[3]);
        break;
    }
    return params - 1;
}

void UISProcessThreadAction(UISInfo_t* info, unsigned char onlyControls) {
    u32* top = info->actionStackBase;
    u32* base = top;
    if (onlyControls) {
        while (top > info->actionStackPtr) {
            UISThreadActionRecord* rec = RecordAt(top);
            u32 action = rec->action;
            u32* next = &RecordParams(top)->value - 1;
            if (action == UIS_TA_ACTIVATE_CONTROL) {
                ActivateControl(info, 1, &rec->group);
            } else if (action == UIS_TA_DEACTIVATE_CONTROL) {
                ActivateControl(info, 0, &rec->group);
            }
            top = next;
        }
        return;
    }
    while (top > info->actionStackPtr) {
        top = &_UISDoThreadAction(info, (UISParam_t*)top)->value;
    }
    info->actionStackPtr = base;
}
