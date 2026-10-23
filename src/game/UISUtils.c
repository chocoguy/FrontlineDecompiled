// UI runtime ("iSiren") helpers: event/hint lookup, script calls, color
// tint, string formatting and screen ordering.
#include "game/UISUtils.h"
#include "game/UISStack.h"
#include "game/uistypes.h"
#include "stdio.h"
#include "string.h"

static float _MultiplerColorFactor[4];
static float _AdditiveColorFactor[4];

int UISFindScreen(UISInfo_t* info, unsigned short group, unsigned short screen) {
    u32 i;
    for (i = 0; i < info->numScreens; i++) {
        UISScreen_t* s = &info->screens[i];
        if (s->group == group && s->id == screen) {
            break;
        }
    }
    return i;
}

int UISFindEventPC(UISControl_t* control, unsigned long event) {
    for (u32 i = 0; i < control->numEvents; i++) {
        UISEventEntry_t* e = &control->events[i];
        if ((e->flags & UISEVENT_EVENT) && e->id == (u16)event) {
            return e->pc;
        }
    }
    return 0;
}

int UISFindSubControlEventPC(UISControl_t* control, unsigned short sub, unsigned long event) {
    for (u32 i = 0; i < control->numEvents; i++) {
        UISEventEntry_t* e = &control->events[i];
        if (!(e->flags & (UISEVENT_EVENT | UISEVENT_HINT)) && e->id == (u16)event &&
            (e->flags & 0x2FFF) == sub) {
            return e->pc;
        }
    }
    return 0;
}

static void* _UISFindHintPC(UISControl_t* control, unsigned long hint) {
    for (u32 i = 0; i < control->numEvents; i++) {
        UISEventEntry_t* e = &control->events[i];
        if ((e->flags & UISEVENT_HINT) && e->id == (u16)hint) {
            return ADDR_TO_PTR(void, e->pc);
        }
    }
    return NULL;
}

// Calls the script at `pc` with (result, [value], params..., params2...,
// control, 0) pushed on the interpreter stack; returns UISStackProcess's
// state (3: suspended waiting for a screen).
signed char UISExecuteFnc(UISInfo_t* info, UISScreen_t* screen, UISControlInfo_t* control,
                   UISStackInfo_t* stack, void* pc, unsigned long numParams, UISParam_t* params,
                   unsigned long numParams2, UISParam_t* params2, unsigned char hasValue,
                   long value, UISParam_t* result) {
    UISParam_t* frame = stack->sp;
    if (result == NULL) {
        frame->value = 0;
    } else {
        frame->value = result->value;
    }
    stack->sp++;
    if (hasValue) {
        stack->sp->value = value;
        stack->sp++;
    }
    for (u32 i = 0; i < numParams; i++) {
        stack->sp->value = params[i].value;
        stack->sp++;
    }
    for (u32 i = 0; i < numParams2; i++) {
        stack->sp->value = params2[i].value;
        stack->sp++;
    }
    stack->sp->value = (u32)PTR_TO_ADDR(control);
    stack->sp++;
    stack->sp->value = 0;
    stack->sp++;
    stack->pc = pc;

    signed char state = UISStackProcess(info, frame, stack, screen, control);
    if (result != NULL) {
        result->value = frame->value;
    }
    if (state != 3) {
        stack->sp = frame;
    }
    return state;
}

float* UISGetActionPtrValue(unsigned long action, UISControlInfo_t* control) {
    u8* p = (u8*)control;
    switch (action) {
    case 16:
        return (float*)(p + 0x20);
    case 17:
        return (float*)(p + 0x24);
    case 18:
        return (float*)(p + 0x28);
    case 19:
        return (float*)(p + 0x8);
    case 20:
        return (float*)(p + 0xC);
    case 21:
        return (float*)(p + 0x10);
    case 22:
        return (float*)(p + 0x2C);
    case 23:
        return (float*)(p + 0x30);
    case 24:
        return (float*)(p + 0x34);
    case 25:
        return (float*)(p + 0x14);
    case 26:
        return (float*)(p + 0x18);
    case 27:
        return (float*)(p + 0x1C);
    case 32:
        return (float*)(p + 0x58);
    case 33:
        return (float*)(p + 0x4C);
    case 34:
        return (float*)(p + 0x50);
    case 35:
        return (float*)(p + 0x54);
    case 36:
        return (float*)(p + 0x48);
    case 37:
        return (float*)(p + 0x3C);
    case 38:
        return (float*)(p + 0x40);
    case 39:
        return (float*)(p + 0x44);
    default:
        return (float*)(p + 0x48);
    }
}

extern "C" float* UISGetColorMultipler() {
    return _MultiplerColorFactor;
}

extern "C" float* UISGetColorAdditive() {
    return _AdditiveColorFactor;
}

void UISSetColorMultipler(float r, float g, float b, float a) {
    _MultiplerColorFactor[3] = a;
    _MultiplerColorFactor[0] = r;
    _MultiplerColorFactor[1] = g;
    _MultiplerColorFactor[2] = b;
}

void UISSetColorAdditive(float r, float g, float b, float a) {
    _AdditiveColorFactor[3] = a;
    _AdditiveColorFactor[0] = r;
    _AdditiveColorFactor[1] = g;
    _AdditiveColorFactor[2] = b;
}

// Supports %s (UISString_t param), %f (float param), %d, %c, %x. The
// arguments are passed to sprintf as plain words, as in the original.
void UISStringFormat(UISScrData_t*, UISString_t* dst, UISString_t* fmt, unsigned long,
                     UISParam_t* params) {
    static char Temp[0x200];
    u32 args[40] = {0};
    int n = 0;
    int p = 0;

    if (fmt == NULL) {
        return;
    }
    char* text = fmt->text;
    u32 len = fmt->maxLength;
    for (u32 i = 0; i < len; i++) {
        if (text[i] != '%') {
            continue;
        }
        u8 scanning = 1;
        char* c = &text[i];
        while (scanning && i++ < len) {
            c++;
            switch (*c) {
            case '%':
                scanning = 0;
                break;
            case 's': {
                UISString_t* s = ADDR_TO_PTR(UISString_t, params[p].value);
                if (s != NULL) {
                    args[n] = (u32)PTR_TO_ADDR(s->text);
                }
                scanning = 0;
                p++;
                n++;
                break;
            }
            case 'f':
                *(double*)&args[n] = *(float*)&params[p].value;
                n += 2;
                scanning = 0;
                p++;
                break;
            case 'd':
            case 'c':
            case 'x':
                args[n] = params[p].value;
                p++;
                scanning = 0;
                n++;
                break;
            }
        }
    }

    if (dst != NULL) {
        char* out = dst->text;
        u32 max = dst->maxLength;
        sprintf(Temp, text, args[0], args[1], args[2], args[3], args[4], args[5], args[6],
                args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14],
                args[15], args[16], args[17], args[18], args[19]);
        strncpy(out, Temp, max);
        out[max] = '\0';
    }
}

static inline bool ListHasControl(UISScrData_t* data, UISControlList_t* list,
                                  UISControlInfo_t* control) {
    for (u32 k = 0; k < list->numEntries; k++) {
        UISListEntry_t* e = &list->entries[k];
        if (e->type == UISLIST_CONTROL && data->controls[e->data].info == control) {
            return true;
        }
    }
    return false;
}

UISControlInfo_t* UISFindSiblingEnableControl(UISScreen_t* screen, UISControlInfo_t* control) {
    UISScrData_t* data = screen->data;
    for (u32 i = 0; i < data->numControls; i++) {
        UISControl_t* c = &data->controls[i];
        bool found = false;
        for (u32 j = 0; j < c->numLists; j++) {
            UISControlList_t* list = c->lists[j];
            if (*list->enable != 0 && ListHasControl(data, list, control)) {
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }
        for (u32 j = 0; j < c->numLists; j++) {
            UISControlList_t* list = c->lists[j];
            if (*list->enable == 0) {
                continue;
            }
            for (u32 k = 0; k < list->numEntries; k++) {
                UISListEntry_t* e = &list->entries[k];
                if (e->type == UISLIST_CONTROL) {
                    UISControlInfo_t* info = data->controls[e->data].info;
                    if (info->enabled == 1) {
                        return info;
                    }
                }
            }
        }
        return NULL;
    }
    return NULL;
}

void UISMoveScreenDrawPosition(UISInfo_t* info, unsigned short group, unsigned short screen,
                               long delta) {
    u32 num = info->numScreens;
    int cur = UISFindScreen(info, group, screen);
    if (cur >= (int)num) {
        return;
    }

    int steps;
    int dir;
    if (delta >= 0) {
        steps = delta;
        dir = 1;
    } else {
        steps = -delta;
        dir = -1;
    }

    while (steps-- != 0) {
        int from = cur;
        cur += dir;
        if (cur >= (int)num || cur < 0) {
            return;
        }
        UISScreen_t* a = &info->screens[cur];
        UISScreen_t* b = &info->screens[from];

        if (info->activeScreen == (u32)cur) {
            info->activeScreen = from;
        } else if (info->activeScreen == (u32)from) {
            info->activeScreen = cur;
        }
        for (u32 i = 0; i < info->numRateFncs; i++) {
            UISScreen_t** s = &info->rateFncs[i].screen;
            if (*s == &info->screens[cur]) {
                *s = &info->screens[from];
            } else if (*s == &info->screens[from]) {
                *s = &info->screens[cur];
            }
        }
        for (u32 i = 0; i < info->numScreenRefs; i++) {
            UISScreen_t** s = &info->screenRefs[i].screen;
            if (*s == &info->screens[cur]) {
                *s = &info->screens[from];
            } else if (*s == &info->screens[from]) {
                *s = &info->screens[cur];
            }
        }

        UISScreen_t tmp;
        memcpy(&tmp, &info->screens[cur], sizeof(UISScreen_t));
        memcpy(&info->screens[cur], &info->screens[from], sizeof(UISScreen_t));
        memcpy(&info->screens[from], &tmp, sizeof(UISScreen_t));
    }
}

// Children first, then the control itself.
static void _ParseHints(UISInfo_t* info, UISScreen_t* screen, UISStackInfo_t* stack,
                        unsigned long index, unsigned long hint, unsigned long numParams,
                        UISParam_t* params) {
    UISControl_t* control = &screen->data->controls[index];
    for (u32 i = 0; i < control->numEvents; i++) {
        UISEventEntry_t* e = &control->events[i];
        if (e->id == UISEVENT_CHILD) {
            _ParseHints(info, screen, stack, e->pc, hint, numParams, params);
        }
    }
    void* pc = _UISFindHintPC(control, hint);
    if (pc != NULL) {
        UISExecuteFnc(info, screen, control->info, stack, pc, numParams, params, 0, NULL, 0, 0,
                      NULL);
    }
}

void UISDoHint(UISInfo_t* info, unsigned long hint, unsigned long numParams, UISParam_t* params) {
    u32 num = info->numScreens;
    for (u32 i = 0; i < num; i++) {
        info->flags |= 2;
        _ParseHints(info, &info->screens[i], &info->stack, 0, hint, numParams, params);
        info->flags &= ~2;
    }
}
