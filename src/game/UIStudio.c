// UI runtime ("iSiren"): screen loading, input events, drawing and the
// control tree walks.
#include "game/UIStudio.h"

#include <string.h>

#include "game/UISActionProcess.h"
#include "game/UISEvent.h"
#include "game/UISStack.h"
#include "game/UISUtils.h"
#include "game/uistypes.h"

extern void (*RuntimeErrorFnc)(int error);

static inline void CallPlugin(UISInfo_t* info, UISListEntry_t* e, unsigned long event,
                              unsigned long numParams, UISParam_t* params) {
    if (e->type < info->numPlugins) {
        UISPluginProcessFnc fnc = info->plugins[e->type];
        if (fnc != NULL) {
            fnc((void*)e->data, event, numParams, params, NULL);
        }
    }
}

// Sends `event` (-1: load, -3: unload) to the screen's plugin objects and to
// those of the control tree under control `index`.
static void _ParseInitialize(UISInfo_t* info, UISScreen_t* screen, unsigned long index,
                             unsigned long event) {
    if (screen->data == NULL) {
        return;
    }
    UISControl_t* control = &screen->data->controls[index];
    s16 loaded = event == (unsigned long)-1;
    for (u32 i = 0; i < screen->data->numObjects; i++) {
        UISListEntry_t* obj = &screen->data->objects[i];
        if (obj->state == 0) {
            CallPlugin(info, obj, event, 0, NULL);
            obj->state = 1;
        }
    }
    for (u32 i = 0; i < control->numLists; i++) {
        UISControlList_t* list = control->lists[i];
        for (u32 j = 0; j < list->numEntries; j++) {
            UISListEntry_t* e = &list->entries[j];
            if (e->type == UISLIST_CONTROL) {
                _ParseInitialize(info, screen, e->data, event);
            } else if (e->type < info->numPlugins) {
                UISPluginProcessFnc fnc = info->plugins[e->type];
                if (fnc != NULL) {
                    fnc((void*)e->data, event, 0, NULL, NULL);
                    e->state = loaded;
                }
            }
        }
    }
}

// Finds the node whose visible/enable word is *target, searching from `node`
// (of type `kind`). On success *target becomes that node (NULL for the
// screen's root control) and its current value is returned; -1 otherwise.
static int _DetermineVisibility(UISScreen_t* screen, void** target, unsigned long kind,
                                void* node) {
    if (target == NULL || *target == NULL || node == NULL || screen == NULL) {
        return -1;
    }
    switch (kind) {
    case UISVIS_SCREEN: {
        UISScrData_t* data = screen->data;
        if (data != NULL && data->controls[0].info == *target) {
            *target = NULL;
            UISControlInfo_t* info = screen->data->controls[0].info;
            if (info != NULL) {
                return info->visible;
            }
            return 0;
        }
        node = data->controls;
    }
        // fallthrough
    case UISVIS_CONTROL: {
        UISControl_t* control = (UISControl_t*)node;
        if (control->info == *target) {
            *target = control;
            if (control->info != NULL) {
                return control->info->visible;
            }
            return 0;
        }
        if (control->info != NULL && control->info->visible != 0) {
            for (u32 i = 0; i < control->numLists; i++) {
                int result = _DetermineVisibility(screen, target, UISVIS_LIST, control->lists[i]);
                if (result != -1) {
                    return result;
                }
            }
        }
        break;
    }
    case UISVIS_LIST: {
        UISControlList_t* list = (UISControlList_t*)node;
        if (list->enable == *target) {
            *target = list;
            if (list->enable != NULL) {
                return *list->enable;
            }
            return 0;
        }
        if (list->enable != NULL && *list->enable != 0) {
            for (u32 i = 0; i < list->numEntries; i++) {
                UISListEntry_t* e = &list->entries[i];
                if (e != NULL && e->type == UISLIST_CONTROL) {
                    int result = _DetermineVisibility(screen, target, UISVIS_CONTROL,
                                                      &screen->data->controls[e->data]);
                    if (result != -1) {
                        return result;
                    }
                }
            }
        }
        break;
    }
    }
    return -1;
}

// Tells the plugin objects under `node` that they are now shown/hidden.
// Only descends through nodes whose state changes (or, below the top, is
// already shown).
static void _ParseVisibility(UISInfo_t* info, UISScreen_t* screen, unsigned long visible,
                             unsigned long kind, void* node, unsigned long depth) {
    if (info == NULL || screen == NULL) {
        return;
    }
    if (node == NULL && kind != UISVIS_SCREEN) {
        return;
    }
    switch (kind) {
    case UISVIS_SCREEN: {
        UISScrData_t* data = screen->data;
        if (data == NULL) {
            return;
        }
        UISControlInfo_t* root = data->controls[0].info;
        if (root == NULL) {
            return;
        }
        u32 v = root->visible;
        if (v == visible) {
            return;
        }
        if (v == 0 && depth != 0) {
            return;
        }
        node = data->controls;
    }
        // fallthrough
    case UISVIS_CONTROL: {
        UISControl_t* control = (UISControl_t*)node;
        if (control->info == NULL) {
            return;
        }
        u32 v = control->info->visible;
        if (v == visible) {
            return;
        }
        if (v == 0 && depth != 0) {
            return;
        }
        for (u32 i = 0; i < control->numLists; i++) {
            _ParseVisibility(info, screen, visible, UISVIS_LIST, control->lists[i], depth + 1);
        }
        break;
    }
    case UISVIS_LIST: {
        UISControlList_t* list = (UISControlList_t*)node;
        if (list->enable == NULL) {
            return;
        }
        u32 v = *list->enable;
        if (v == visible) {
            return;
        }
        if (v == 0 && depth != 0) {
            return;
        }
        for (u32 i = 0; i < list->numEntries; i++) {
            UISListEntry_t* e = &list->entries[i];
            if (e != NULL && e->type == UISLIST_CONTROL) {
                _ParseVisibility(info, screen, visible, UISVIS_CONTROL,
                                 &screen->data->controls[e->data], depth + 1);
            } else {
                CallPlugin(info, e, UIS_EV_SHOW, 1, (UISParam_t*)&visible);
            }
        }
        break;
    }
    }
}

// Applies (or ends) each shown control's transform, top down.
static void _ParseTransforms(UISInfo_t* info, UISTransformAction action, UISScreen_t* screen,
                             unsigned long index) {
    if (screen->data == NULL) {
        return;
    }
    UISControl_t* control = &screen->data->controls[index];
    info->transform(action, &control->info->transform);
    for (u32 i = 0; i < control->numLists; i++) {
        UISControlList_t* list = control->lists[i];
        if (*list->enable != 0) {
            for (u32 j = 0; j < list->numEntries; j++) {
                UISListEntry_t* e = &list->entries[j];
                if (e->type == UISLIST_CONTROL) {
                    _ParseTransforms(info, action, screen, e->data);
                }
            }
        }
    }
}

// Sends `event` to the screen's plugin objects, then draws the control tree
// under control `index` with each control's tint and transform applied.
static void _ParseObjects(UISInfo_t* info, UISScreen_t* screen, unsigned long index,
                          unsigned long event) {
    float mul[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float add[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (screen->data == NULL) {
        return;
    }
    UISControl_t* control = &screen->data->controls[index];
    for (u32 i = 0; i < screen->data->numObjects; i++) {
        CallPlugin(info, &screen->data->objects[i], event, 0, NULL);
    }
    if (control->info->visible == 0) {
        return;
    }

    float* m = UISGetColorMultipler();
    mul[0] = m[0];
    mul[1] = m[1];
    mul[2] = m[2];
    mul[3] = m[3];
    float* a = UISGetColorAdditive();
    UISControlInfo_t* ci = control->info;
    add[0] = a[0];
    add[1] = a[1];
    add[2] = a[2];
    add[3] = a[3];
    UISSetColorMultipler(mul[0] * ci->colorMul[0], mul[1] * ci->colorMul[1],
                         mul[2] * ci->colorMul[2], mul[3] * ci->colorMul[3]);
    UISSetColorAdditive(add[0] + ci->colorAdd[0], add[1] + ci->colorAdd[1],
                        add[2] + ci->colorAdd[2], add[3] + ci->colorAdd[3]);
    info->transform(UIS_TRANSFORM_BEGIN, &control->info->transform);
    for (u32 i = 0; i < control->numLists; i++) {
        UISControlList_t* list = control->lists[i];
        if (*list->enable == 0) {
            continue;
        }
        for (u32 j = 0; j < list->numEntries; j++) {
            UISListEntry_t* e = &list->entries[j];
            if (e->type == UISLIST_CONTROL) {
                _ParseObjects(info, screen, e->data, event);
            } else if (e->state != 0) {
                CallPlugin(info, e, event, 0, NULL);
            }
        }
    }
    UISSetColorMultipler(mul[0], mul[1], mul[2], mul[3]);
    UISSetColorAdditive(add[0], add[1], add[2], add[3]);
    info->transform(UIS_TRANSFORM_END, &control->info->transform);
}

// Runs `event` on the control tree under control `index`: children first
// (then the parent's sub-control handler for each enabled child), then the
// control's own handler. Only enabled, input-accepting controls get input
// events. *enabled receives the control's enabled state. Returns the last
// script's state (2 aborts the walk).
static int _ParseMaps(UISInfo_t* info, UISScreen_t* screen, UISStackInfo_t* stack,
                      unsigned long index, unsigned long port, unsigned long event,
                      unsigned long numParams, UISParam_t* params, signed char* enabled) {
    int result = 0;
    UISScrData_t* data = screen->data;
    if (data == NULL || index >= data->numControls) {
        return 0;
    }
    UISControl_t* control = &data->controls[index];
    UISControlInfo_t* ci = control->info;
    if ((ci->enabled != 0 && ci->acceptsInput != 0) || event + 10 <= 2 || event + 5 <= 3 ||
        event == (unsigned long)-11) {
        for (u32 i = 0; i < control->numEvents; i++) {
            UISEventEntry_t* e = &control->events[i];
            if (e->id == UISEVENT_CHILD) {
                signed char childEnabled = 0;
                result = _ParseMaps(info, screen, stack, e->pc, port, event, numParams, params,
                                    &childEnabled);
                if (childEnabled == 1) {
                    void* pc = (void*)UISFindSubControlEventPC(control, e->pc, event);
                    if (pc != NULL) {
                        if (UISExecuteFnc(info, screen, control->info, stack, pc, numParams,
                                          params, 0, NULL, 1, index, NULL) == 2) {
                            return 2;
                        }
                    }
                }
            }
        }
        UISProcessThreadAction(info, 1);
        void* pc = (void*)UISFindEventPC(control, event);
        if ((event == (unsigned long)UIS_EV_CONTROL_ACTIVATE ||
             event == (unsigned long)UIS_EV_CONTROL_DEACTIVATE) &&
            control->info != (UISControlInfo_t*)params[2].value) {
            pc = NULL;
        }
        if (pc != NULL) {
            result = UISExecuteFnc(info, screen, control->info, stack, pc, numParams, params, 0,
                                   NULL, 1, port, NULL);
        }
    }
    *enabled = control->info->enabled;
    return result;
}

// Advances the running rate functions by `ticks`: each whole interval runs
// the function's script (which returns a step scale) and steps its value
// towards the target; on arrival the function finishes and runs its done
// script.
static void _ParseRateFncs(UISInfo_t* info, unsigned long ticks) {
    UISRemoveUnNessaryRateFncs(info);
    UISStackInfo_t* stack = &info->rateStack;
    info->flags |= UISINFO_FLAG_RATEFNCS;
    u32 num = info->numRateFncs;
    for (u32 i = 0; i < num; i++) {
        UISRateFnc_t* fnc = &info->rateFncs[i];
        if (fnc->state != RATEFNC_RUNNING) {
            continue;
        }
        UISScreen_t* screen = fnc->screen;
        if (fnc->interval == 0) {
            continue;
        }
        fnc->ticks += ticks;
        s32 pending = fnc->ticks - fnc->ticksDone;
        while (pending >= (s32)fnc->interval) {
            float scale;
            pending -= fnc->interval;
            fnc->ticksDone += fnc->interval;
            if (fnc->data != NULL) {
                if (fnc->state != RATEFNC_RUNNING) {
                    break;
                }
                UISParam_t result;
                UISParam_t params[3];
                unsigned long numParams;
                *(float*)&result.value = 1.0f;
                params[0].value = fnc->id;
                params[1].value = fnc->ticksDone;
                if (fnc->action == 0) {
                    numParams = 3;
                    params[2].value = fnc->interval;
                } else {
                    numParams = 2;
                }
                UISExecuteFnc(info, screen, fnc->control, stack, fnc->data, numParams, params, 0,
                              NULL, 0, -1, &result);
                scale = *(float*)&result.value;
            } else {
                scale = 1.0f;
            }
            if (fnc->action == 0) {
                continue;
            }
            float* value = UISGetActionPtrValue(fnc->action, fnc->control2);
            float rate = fnc->rate;
            if (scale < 0.0f) {
                scale = 1.0f;
            }
            float next = rate * scale + *value;
            if ((rate > 0.0f && next < fnc->target) || (rate < 0.0f && next > fnc->target)) {
                *value = next;
            } else {
                *value = fnc->target;
                fnc->state = RATEFNC_FINISHED;
                if (fnc->data2 != NULL) {
                    UISParam_t param;
                    param.value = fnc->id;
                    UISExecuteFnc(info, screen, fnc->control, stack, fnc->data2, 1, &param, 0,
                                  NULL, 0, -1, NULL);
                }
            }
        }
    }
    info->flags &= ~UISINFO_FLAG_RATEFNCS;
    UISRemoveUnNessaryRateFncs(info);
}

#define PATCH(base, p) (*(u32*)&(p) = (u32)(base) + *(u32*)&(p))

// Turns a loaded screen file's offsets into pointers. Returns 1, or -1 if
// it was already patched.
int PatchScrData(UISScrData_t* data) {
    if ((u32)data->controls >= (u32)data) {
        return -1;
    }
    u8* base = (u8*)data;
    PATCH(base, data->controls);
    for (u32 i = 0; i < data->numControls; i++) {
        UISControl_t* control = &data->controls[i];
        PATCH(base, control->info);
        PATCH(base, control->lists);
        u32 j = control->numLists;
        while (j-- != 0) {
            PATCH(base, control->lists[j]);
            UISControlList_t* list = control->lists[j];
            PATCH(base, list->enable);
            PATCH(base, list->entries);
            u32 k = list->numEntries;
            while (k-- != 0) {
                UISListEntry_t* e = &list->entries[k];
                if (e->type != UISLIST_CONTROL) {
                    e->state = 0;
                    PATCH(base, e->data);
                }
            }
        }
        PATCH(base, control->events);
        j = control->numEvents;
        while (j-- != 0) {
            UISEventEntry_t* e = &control->events[j];
            if (e->id != UISEVENT_CHILD) {
                PATCH(base, e->pc);
            }
        }
    }
    PATCH(base, data->strings);
    u32 i = data->numStrings;
    while (i-- != 0) {
        PATCH(base, data->strings[i].text);
    }
    PATCH(base, data->stringRefs);
    i = data->numStringRefs;
    while (i-- != 0) {
        u32* ref = (u32*)(base + data->stringRefs[i]);
        if (*ref < data->numStrings) {
            *ref = (u32)&data->strings[*ref];
        } else {
            *ref = 0;
        }
    }
    PATCH(base, data->objects);
    i = data->numObjects;
    while (i-- != 0) {
        if (data->objects[i].data != 0) {
            PATCH(base, data->objects[i].data);
        }
    }
    return 1;
}

unsigned long UISGetMemSize(unsigned long maxScreens, unsigned long maxPlugins,
                            unsigned long maxRateFncs, unsigned long maxScreenRefs,
                            unsigned long stackSize, unsigned long rateStackSize) {
    return maxScreens * sizeof(UISScreen_t) + maxPlugins * sizeof(UISPluginProcessFnc) +
           maxRateFncs * sizeof(UISRateFnc_t) + maxScreenRefs * sizeof(UISScreenRef_t) +
           (stackSize + rateStackSize) * sizeof(UISParam_t) + sizeof(UISInfo_t);
}

// `ui` is a block of UISGetMemSize bytes; the arrays follow the UISInfo_t.
void UISInit(UISInfo_t* ui, unsigned long maxScreens, unsigned long maxPlugins,
             unsigned long maxRateFncs, unsigned long maxScreenRefs, unsigned long stackSize,
             unsigned long rateStackSize, unsigned long ticksPerSecond) {
    u8* mem = (u8*)ui;
    u32 offset = sizeof(UISInfo_t) + maxScreens * sizeof(UISScreen_t);
    ui->magic = UIS_MAGIC;
    ui->flags = 0;
    ui->maxScreens = maxScreens;
    ui->numScreens = 0;
    ui->screens = (UISScreen_t*)(mem + sizeof(UISInfo_t));
    ui->maxPlugins = maxPlugins;
    ui->numPlugins = 0;
    ui->plugins = (UISPluginProcessFnc*)(mem + offset);
    offset += maxPlugins * sizeof(UISPluginProcessFnc);
    ui->maxRateFncs = maxRateFncs;
    ui->numRateFncs = 0;
    ui->rateFncs = (UISRateFnc_t*)(mem + offset);
    offset += maxRateFncs * sizeof(UISRateFnc_t);
    ui->maxScreenRefs = maxScreenRefs;
    ui->numScreenRefs = 0;
    ui->screenRefs = (UISScreenRef_t*)(mem + offset);
    offset += maxScreenRefs * sizeof(UISScreenRef_t);
    UISParam_t* stack = (UISParam_t*)(mem + offset);
    offset += stackSize * sizeof(UISParam_t);
    UISParam_t* rateStack = (UISParam_t*)(mem + offset);
    ui->stack.sp = stack;
    ui->stack.base = stack;
    ui->stack.bottom = stack;
    ui->stack.end = ui->stack.base + stackSize;
    ui->actionStackBase = &ui->stack.end[-1].value;
    ui->actionStackPtr = &ui->stack.end[-1].value;
    ui->stackBase = &ui->stack.base;
    ui->rateStack.sp = rateStack;
    ui->rateStack.base = rateStack;
    ui->rateStack.bottom = rateStack;
    ui->rateStack.end = ui->rateStack.base + rateStackSize;
    ui->ticksPerSecond = ticksPerSecond;
    ui->activeScreen = (u32)-1;
    ui->gameMessage = NULL;
    ui->transform = NULL;
    ui->field_0x1C = 0;
    ui->loadScreen = NULL;
    ui->unloadScreen = NULL;
    RuntimeErrorFnc = NULL;
    ui->scriptFlags = (u32)-1;
}

void UISShutdown(UISInfo_t* ui) {
    for (int i = ui->numScreens - 1; i >= 0; i--) {
        UISScreen_t* screen = &ui->screens[i];
        UISThreadGroupInfoT g;
        UISParam_t param;
        param.value = 0;
        g.h[0] = screen->group;
        g.h[1] = screen->id;
        UISAddThreadAction(ui, UIS_TA_UNLOAD_SCREEN, &g, 1, &param);
        UISProcessThreadAction(ui, 0);
    }
    uisEventNotProcess = 0;
    ui->magic = 0;
}

void UISRegisterMessageFnc(UISInfo_t* ui, UISMessageFnc fnc) {
    ui->gameMessage = fnc;
}

void UISRegisterResourceFncs(UISInfo_t* ui, UISLoadScreenFnc load, UISUnloadScreenFnc unload) {
    ui->loadScreen = load;
    ui->unloadScreen = unload;
}

void UISRegisterTransformFncs(UISInfo_t* ui, UISTransformFnc fnc) {
    ui->transform = fnc;
}

void UISRegisterPluginFnc(UISInfo_t* ui, unsigned long type, UISPluginFnc fnc) {
    ui->plugins[type] = fnc;
    ui->numPlugins++;
}

// Loads a screen file into the next slot and runs its init and load events.
// Returns 0 if it is already loaded or the file is missing.
int UISInternalLoadScreen(UISInfo_t* info, unsigned short group, unsigned short screen,
                          unsigned short parentGroup, unsigned short parentId,
                          unsigned char numParams, UISParam_t* params) {
    u32 index = UISFindScreen(info, group, screen);
    if (index < info->numScreens) {
        return 0;
    }
    UISScrData_t* data = (UISScrData_t*)info->loadScreen(group, screen);
    if (data == NULL) {
        return 0;
    }
    info->numScreens++;
    UISScreen_t* s = &info->screens[index];
    s->group = group;
    s->id = screen;
    s->parentGroup = parentGroup;
    s->parentId = parentId;
    s->data = data;
    int patched = PatchScrData(s->data);
    _ParseInitialize(info, s, 0, (unsigned long)-1);
    s->data->controls[0].info->enabled = 1;
    signed char enabled;
    if (patched != 0) {
        enabled = 0;
        info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
        _ParseMaps(info, s, &info->stack, 0, (unsigned long)-1, (unsigned long)UIS_EV_INIT, 0,
                   NULL, &enabled);
        info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
    }
    _ParseTransforms(info, UIS_TRANSFORM_NONE, s, 0);
    enabled = 0;
    if (numParams == 0xFF) {
        numParams = params[10].value;
    }
    info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
    _ParseMaps(info, s, &info->stack, 0, (unsigned long)-1, (unsigned long)UIS_EV_LOAD, numParams,
               params, &enabled);
    info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
    return 1;
}

// Loads a screen; a modal load (or any load while a thread waits) records a
// screen reference so the active screen comes back when it is unloaded.
static int _UISInternalLoad(UISInfo_t* info, unsigned short group, unsigned short screen,
                            unsigned char modal, unsigned char numParams, UISParam_t* params) {
    u32 refIndex = 0;
    s16 parentGroup;
    s16 parentId;
    if (info->numScreens == 0 || info->activeScreen == (u32)-1) {
        parentGroup = -1;
        parentId = -1;
    } else {
        UISScreen_t* active = &info->screens[info->activeScreen];
        parentGroup = active->group;
        parentId = active->id;
    }
    u32 loaded = (u32)UISFindScreen(info, group, screen) < info->numScreens;
    if (loaded == 1) {
        return 0;
    }
    if (info->numScreenRefs != 0 && loaded == 0) {
        modal = 1;
    }
    if (modal) {
        refIndex = info->numScreenRefs;
        UISScreenRef_t* ref = &info->screenRefs[refIndex];
        memset(ref, 0, sizeof(UISScreenRef_t));
        ref->group = group;
        ref->id = screen;
        info->numScreenRefs++;
    }
    int result = UISInternalLoadScreen(info, group, screen, parentGroup, parentId, numParams,
                                       params);
    if (modal) {
        int index = UISFindScreen(info, group, screen);
        if (index < (int)info->numScreens) {
            UISScreenRef_t* ref = &info->screenRefs[refIndex];
            ref->control = info->screens[index].data->controls[0].info;
            int parent = UISFindScreen(info, parentGroup, parentId);
            if (parent < (int)info->numScreens) {
                ref->screen = &info->screens[parent];
            }
        } else {
            info->numScreenRefs--;
        }
    }
    return result;
}

int UISLoadScreen(UISInfo_t* ui, unsigned short group, unsigned short screen,
                  unsigned long numParams, UISParam_t* params) {
    if (ui->flags & UISINFO_FLAG_INTERNAL_EVENTS) {
        UISThreadGroupInfoT g;
        g.h[0] = group;
        g.h[1] = screen;
        g.h[2] = 0xFFFF;
        g.h[3] = 0xFFFF;
        UISAddThreadAction(ui, UIS_TA_LOAD_SCREEN, &g, (u8)numParams, params);
        return 1;
    }
    return _UISInternalLoad(ui, group, screen, 0, numParams, params);
}

// Unloading a screen that a suspended thread waits on resumes that thread
// with `value` as the wait's result. Returns 0 if another wait still needs
// the screen.
static bool UISInternalUnloadModal(UISInfo_t* info, unsigned short group, unsigned short screen,
                                   long value) {
    s32 num = info->numScreenRefs;
    if (num <= 0) {
        return true;
    }
    UISScreenRef_t* ref = &info->screenRefs[num - 1];
    if (ref->id == screen && ref->group == group) {
        info->numScreenRefs = num - 1;
        if (ref->screen == NULL) {
            info->activeScreen = (u32)-1;
            return true;
        }
        info->activeScreen = UISFindScreen(info, ref->screen->group, ref->screen->id);
        if (info->activeScreen < info->numScreens) {
            UISParam_t* frame = ref->frame;
            if (frame != NULL) {
                UISStackInfo_t* stack = ref->stack;
                stack->bottom = ref->bottom;
                stack->base = ref->base;
                stack->end = ref->end;
                stack->sp = ref->sp;
                stack->pc = ref->pc;
                stack->sp[-1].value = value;
                if (UISStackProcess(info, ref->frame, stack, ref->screen, ref->control) != 3) {
                    stack->sp = frame;
                }
            }
        }
        return true;
    }
    for (s32 i = num; i-- != 0;) {
        UISScreenRef_t* r = &info->screenRefs[i];
        if (r->id == screen && r->group == group) {
            return false;
        }
        if (r->screen != NULL && r->screen->id == screen && r->screen->group == group) {
            return false;
        }
    }
    return true;
}

bool UISInternalUnloadScreen(UISInfo_t* info, unsigned short group, unsigned short screen,
                             long value) {
    u32 index = UISFindScreen(info, group, screen);
    if (index >= info->numScreens) {
        return true;
    }
    UISScreen_t* s = &info->screens[index];
    u16 parentGroup = s->parentGroup;
    u16 parentId = s->parentId;
    if (!UISInternalUnloadModal(info, group, screen, value)) {
        return false;
    }
    if (index == info->activeScreen) {
        info->activeScreen = (u32)-1;
    }
    // Screens opened from this one now belong to its parent.
    for (u32 i = 0; i < info->numScreens; i++) {
        UISScreen_t* other = &info->screens[i];
        if (other->parentGroup == group && other->parentId == screen) {
            other->parentGroup = parentGroup;
            other->parentId = parentId;
        }
    }
    u32 i = info->numRateFncs;
    while (i-- != 0) {
        if (info->rateFncs[i].screen == s) {
            info->rateFncs[i].state = RATEFNC_FINISHED;
        }
    }
    UISRemoveUnNessaryRateFncs(info);
    signed char enabled = 0;
    info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
    _ParseMaps(info, s, &info->stack, 0, (unsigned long)-1, (unsigned long)UIS_EV_UNLOAD, 0,
               NULL, &enabled);
    info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
    _ParseInitialize(info, s, 0, (unsigned long)UIS_EV_UNLOAD);
    _ParseTransforms(info, (UISTransformAction)3, s, 0);
    info->unloadScreen(s->group, s->id, s->data);
    s->data = NULL;
    info->numScreens--;

    // Close the gap, fixing pointers to the moved screens.
    for (; index < info->numScreens; index++) {
        UISScreen_t* dst = &info->screens[index];
        UISScreen_t* src = &info->screens[index + 1];
        memmove(dst, src, sizeof(UISScreen_t));
        u32 j = info->numScreenRefs;
        while (j-- != 0) {
            if (info->screenRefs[j].screen == src) {
                info->screenRefs[j].screen = dst;
            }
        }
        j = info->maxRateFncs;
        while (j-- != 0) {
            if (info->rateFncs[j].screen == src) {
                info->rateFncs[j].screen = dst;
            }
        }
        if (index + 1 == info->activeScreen) {
            info->activeScreen = index;
        }
    }

    // Activate the parent (or the top screen) if no screen is active.
    if (info->activeScreen == (u32)-1) {
        if (info->numScreens != 0) {
            UISScreen_t* next;
            info->activeScreen = UISFindScreen(info, parentGroup, parentId);
            if (info->activeScreen < info->numScreens) {
                next = &info->screens[info->activeScreen];
            } else {
                info->activeScreen = info->numScreens - 1;
                next = &info->screens[info->activeScreen];
            }
            if (next != NULL) {
                UISThreadGroupInfoT g;
                g.h[0] = next->group;
                g.h[1] = next->id;
                UISAddThreadAction(info, UIS_TA_ACTIVATE_SCREEN, &g, 0, NULL);
            }
        } else {
            info->activeScreen = (u32)-1;
        }
    }
    return true;
}

void UISSetScreenActive(UISInfo_t* ui, unsigned short group, unsigned short screen) {
    UISThreadGroupInfoT g;
    g.h[0] = group;
    g.h[1] = screen;
    UISAddThreadAction(ui, UIS_TA_ACTIVATE_SCREEN, &g, 0, NULL);
    if (!(ui->flags & UISINFO_FLAG_INTERNAL_EVENTS)) {
        UISProcessThreadAction(ui, 0);
    }
}

// Sends input `event` from `port` to the active screen (or all screens) if
// the port is enabled. While a modal wait is pending, its screen's parent
// is the active one.
void UISProcessEvent(UISInfo_t* ui, long port, unsigned long event, unsigned long numValues,
                     float* values, unsigned char allScreens) {
    u32 i;
    u32 end;
    UISProcessThreadAction(ui, 0);
    if (!(ui->scriptFlags & (1 << port))) {
        return;
    }
    int top = ui->numScreenRefs - 1;
    if (top >= 0) {
        UISScreenRef_t* ref = &ui->screenRefs[top];
        ui->activeScreen = UISFindScreen(ui, ref->group, ref->id);
    }
    if (allScreens) {
        end = ui->numScreens;
        i = 0;
    } else {
        i = ui->activeScreen;
        end = i + 1;
        if (i == (u32)-1) {
            return;
        }
    }
    for (; i < end; i++) {
        if ((uisEventNotProcess == 1 && (long)event < 0) || (s8)uisEventNotProcess == 0) {
            signed char enabled = 0;
            ui->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
            _ParseMaps(ui, &ui->screens[i], &ui->stack, 0, port, event, numValues,
                       (UISParam_t*)values, &enabled);
            ui->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
        }
    }
}

void UISProcessInternalEvents(UISInfo_t* info, UISStackInfo_t* stack, long port,
                              unsigned long event, unsigned long numParams, UISParam_t* params,
                              unsigned char allScreens) {
    u32 i;
    u32 end;
    if (allScreens) {
        end = info->numScreens;
        i = 0;
    } else {
        i = info->activeScreen;
        end = i + 1;
        if (i == (u32)-1) {
            return;
        }
    }
    for (; i < end; i++) {
        signed char enabled = 0;
        _ParseMaps(info, &info->screens[i], stack, 0, port, event, numParams, params, &enabled);
    }
}

void UISDrawObjects(UISInfo_t* ui, unsigned long arg) {
    u32 ticks = ui->ticksPerSecond * arg;
    UISSetColorMultipler(1.0f, 1.0f, 1.0f, 1.0f);
    UISSetColorAdditive(0.0f, 0.0f, 0.0f, 0.0f);
    _ParseRateFncs(ui, ticks);
    u32 num = ui->numScreens;
    for (u32 i = 0; i < num; i++) {
        _ParseObjects(ui, &ui->screens[i], 0, (unsigned long)UIS_EV_DRAW);
    }
}

void UISIdleProcess(UISInfo_t* ui, unsigned long frames) {
    UISProcessThreadAction(ui, 0);
    int num = ui->numScreens;
    for (int i = 0; i < num; i++) {
        signed char enabled = 0;
        ui->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
        _ParseMaps(ui, &ui->screens[i], &ui->stack, 0, frames, (unsigned long)UIS_EV_IDLE, 0, NULL,
                   &enabled);
        ui->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
    }
}

// (De)activates `control`: activating first deactivates its enabled
// sibling. Runs the control's (de)activate handler and its parent's
// sub-control handler with `value`.
void UISInternalActivateControl(UISInfo_t* info, unsigned char activate, unsigned long value,
                                UISControlInfo_t* control, UISControlList_t* list,
                                unsigned short screen, unsigned short group) {
    UISControl_t* owner = NULL;
    u32 index = UISFindScreen(info, group, screen);
    if (index >= info->numScreens) {
        return;
    }
    UISScreen_t* s = &info->screens[index];
    unsigned long event = (activate == 1) - 7;  // UIS_EV_CONTROL_(DE)ACTIVATE
    UISScrData_t* data = s->data;
    u32 num = data->numControls;
    u32 i = num;
    while (i-- != 0) {
        UISControl_t* c = &data->controls[i];
        if (c->info == control) {
            owner = c;
            break;
        }
    }
    UISControl_t* parent = NULL;
    u32 j = num;
    while (j-- != 0) {
        UISControl_t* c = &data->controls[j];
        for (int k = 0; k < (int)c->numEvents; k++) {
            u16 flags = c->events[k].flags;
            if (!(flags & (UISEVENT_EVENT | UISEVENT_HINT)) && (flags & 0x2FFF) == (s16)i) {
                parent = c;
                break;
            }
        }
    }
    if (activate == 1) {
        UISControlInfo_t* sibling = UISFindSiblingEnableControl(s, control);
        if (sibling != NULL) {
            UISInternalActivateControl(info, 0, 0, sibling, NULL, screen, group);
        }
    }
    void* pc = (void*)UISFindEventPC(owner, event);
    owner->info->enabled = activate;
    if (pc != NULL) {
        UISExecuteFnc(info, s, owner->info, &info->stack, pc, 0, NULL, 0, NULL, 1, value, NULL);
    }
    if (parent != NULL) {
        pc = (void*)UISFindSubControlEventPC(parent, i, event);
        if (pc != NULL) {
            UISExecuteFnc(info, s, parent->info, &info->stack, pc, 0, NULL, 0, NULL, 1, value,
                          NULL);
        }
    }
}

// Deactivates the active screen (unless a modal wait is pending), then
// makes screen (group, screen) active and visible.
void UISInternalActivateScreen(UISInfo_t* info, unsigned char activate, unsigned short group,
                               unsigned short screen) {
    if (info->numScreenRefs == 0) {
        info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
        u32 i = info->activeScreen;
        u32 end = i + 1;
        if (i != (u32)-1) {
            for (; i < end; i++) {
                signed char enabled = 0;
                _ParseMaps(info, &info->screens[i], &info->stack, 0, 0,
                           (unsigned long)UIS_EV_SCREEN_DEACTIVATE, 0, NULL, &enabled);
            }
        }
        info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
    }
    if (!activate) {
        return;
    }
    u32 index = UISFindScreen(info, group, screen);
    if (index >= info->numScreens) {
        return;
    }
    UISScreen_t* s = &info->screens[index];
    s->data->controls[0].info->enabled = 1;
    s->data->controls[0].info->visible = 1;
    info->activeScreen = index;
    info->flags |= UISINFO_FLAG_INTERNAL_EVENTS;
    u32 i = info->activeScreen;
    u32 end = i + 1;
    if (i != (u32)-1) {
        for (; i < end; i++) {
            signed char enabled = 0;
            _ParseMaps(info, &info->screens[i], &info->stack, 0, 0,
                       (unsigned long)UIS_EV_SCREEN_ACTIVATE, 0, NULL, &enabled);
        }
    }
    info->flags &= ~UISINFO_FLAG_INTERNAL_EVENTS;
}

void UISUpdateVisibility(UISInfo_t* info, UISScreen_t* screen, unsigned long kind, void* var,
                         unsigned long visible) {
    void* node = var;
    int current = _DetermineVisibility(screen, &node, UISVIS_SCREEN, screen);
    u32 want = visible != 0;
    if (current != -1 && (current ^ want) != 0) {
        _ParseVisibility(info, screen, want, kind, node, 0);
    }
}
