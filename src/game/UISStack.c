#include "game/UISStack.h"

#include <string.h>

#include "game/UISActionProcess.h"
#include "game/UISEvent.h"
#include "game/UISUtils.h"
#include "game/UIStudio.h"
#include "game/uistypes.h"

// Set by script opcodes 0x4F/0x50: the event being handled is (not) consumed.
u8 uisEventNotProcess;

// Arguments for UISStringFormat (opcode 0x4C); extra arguments are dropped.
static UISParam_t uisFormatStringStack[20];

// The interpreter is too big for MWCC to keep inlining calls, so the
// helpers are macros. Script code is big-endian bytes.
#define READ_U8(stack) (*(*(u8**)&(stack)->pc)++)

#define READ_U32(stack, out)                                                   \
    do {                                                                       \
        u32 b0_ = READ_U8(stack);                                              \
        u32 b1_ = READ_U8(stack);                                              \
        u32 b2_ = READ_U8(stack);                                              \
        u32 b3_ = READ_U8(stack);                                              \
        (out) = (b0_ << 24) | (b1_ << 16) | (b2_ << 8) | b3_;                  \
    } while (0)

#define READ_S16(stack, out)                                                   \
    do {                                                                       \
        u32 b0_ = READ_U8(stack);                                              \
        u32 b1_ = READ_U8(stack);                                              \
        (out) = (s16)((b0_ << 8) | b1_);                                       \
    } while (0)

#define POP(stack) ((--(stack)->sp)->value)
#define POPF(stack) (*(f32*)&(--(stack)->sp)->value)

#define PUSH(stack, v)                                                         \
    do {                                                                       \
        (stack)->sp->value = (v);                                              \
        (stack)->sp++;                                                         \
    } while (0)

#define PUSHF(stack, v)                                                        \
    do {                                                                       \
        *(f32*)&(stack)->sp->value = (v);                                      \
        (stack)->sp++;                                                         \
    } while (0)

// A variable reference in the code: a 32-bit offset from the pointer held
// in stack slot sp[slot] (slot is negative for locals and arguments).
#define READ_VAR_ADDR(stack, out)                                              \
    do {                                                                       \
        u32 offset_;                                                           \
        s16 slot_;                                                             \
        READ_U32(stack, offset_);                                              \
        READ_S16(stack, slot_);                                                \
        (out) = (u32*)(offset_ + (stack)->sp[slot_].value);                    \
    } while (0)

// Array layout: [numDims, dim sizes..., elements...]. Indices sit on the
// stack below `skip` slots, first dimension first; each is clamped.
#define ARRAY_ELEMENT(top, array, skip, out)                                   \
    do {                                                                       \
        s32 numDims_ = (array)[0];                                             \
        s32 offset_ = 0;                                                       \
        s32 scale_ = 1;                                                        \
        for (s32 i_ = 1; i_ <= numDims_; i_++) {                               \
            s32 size_ = (array)[i_];                                           \
            s32 index_ = ((s32*)(top))[-(i_ + (skip))];                        \
            if (index_ >= size_ || index_ < 0) {                               \
                index_ = size_ - 1;                                            \
            }                                                                  \
            offset_ += scale_ * index_;                                        \
            scale_ *= size_;                                                   \
        }                                                                      \
        (out) = (u32*)&(array)[numDims_ + offset_ + 1];                        \
    } while (0)

// Group info for a screen handle (low half group, high half id) plus the
// current screen.
#define SET_SCREEN_GROUP(g, handle, screen)                                    \
    do {                                                                       \
        u32 h_ = (handle);                                                     \
        (g).h[0] = h_;                                                         \
        (g).h[1] = h_ >> 16;                                                   \
        (g).h[2] = (screen)->group;                                            \
        (g).h[3] = (screen)->id;                                               \
    } while (0)

#define SET_CONTROL_GROUP(g, id_, list_, ctrl_, scr_)                        \
    do {                                                                       \
        (g).h[0] = (id_);                                                       \
        (g).h[1] = 0;                                                          \
        (g).w[1] = (list_);                                                     \
        (g).w[2] = (ctrl_);                                                  \
        (g).h[7] = (scr_)->group;                                            \
        (g).h[6] = (scr_)->id;                                               \
    } while (0)

signed char UISStackProcess(UISInfo_t* info, UISParam_t* frame, UISStackInfo_t* stack,
                            UISScreen_t* screen, UISControlInfo_t* control) {
    UISThreadGroupInfoT g;
    u8* data;

    while (stack->pc != NULL) {
        u8 op = *(u8*)stack->pc;
        u32* top = &stack->sp->value;
        stack->pc = (u8*)stack->pc + 1;
        s32* topi = (s32*)top;
        f32* topf = (f32*)top;

        switch (op) {
        case 0x02:    // load screen: handle below n params
        case 0x71: {
            u8 n = READ_U8(stack);
            UISParam_t* params = stack->sp - n;
            SET_SCREEN_GROUP(g, params[-1].value, screen);
            UISAddThreadAction(info, UIS_TA_LOAD_SCREEN, &g, n, params);
            while (n--) {
                stack->sp--;
            }
            stack->sp--;
            break;
        }
        case 0x03: {  // unload screen
            u32 arg = POP(stack);
            u32 handle = POP(stack);
            u16 group = handle;
            u16 id = handle >> 16;
            if (group == 0xFFFF) {
                group = screen->group;
                id = screen->id;
            }
            g.h[0] = group;
            g.h[1] = id;
            g.w[2] = arg;
            UISAddThreadAction(info, UIS_TA_UNLOAD_SCREEN, &g, 0, NULL);
            break;
        }
        case 0x08: {  // activate screen
            u32 handle = POP(stack);
            u16 group = handle;
            u16 id = handle >> 16;
            if (group == 0xFFFF) {
                group = screen->group;
                id = screen->id;
            }
            g.h[0] = group;
            g.h[1] = id;
            UISAddThreadAction(info, UIS_TA_ACTIVATE_SCREEN, &g, 0, NULL);
            break;
        }
        case 0x58: {  // advanced rate function
            u8 mode = READ_U8(stack);
            u8* data2 = NULL;
            u8* rateData = NULL;
            UISControlInfo_t* ctrl = (UISControlInfo_t*)POP(stack);
            UISControlInfo_t* ctrl2 = (UISControlInfo_t*)POP(stack);
            if (mode >= 5) {
                if (mode >= 6) {
                    rateData = (u8*)screen->data + POP(stack);
                }
                data2 = (u8*)screen->data + POP(stack);
            }
            u32 ticks = POP(stack);
            f32 target = POPF(stack);
            u32 action = POP(stack);
            u32 id = POP(stack);
            UISLoadAdvRateFnc(info, screen, ctrl, ctrl2, id, data2, rateData, ticks, target, action);
            break;
        }
        case 0x06: {  // rate function
            UISControlInfo_t* ctrl = (UISControlInfo_t*)POP(stack);
            u32 arg = POP(stack);
            u32 offset = POP(stack);
            u32 id = POP(stack);
            UISLoadRateFnc(info, screen, ctrl, id, (u8*)screen->data + offset, arg);
            break;
        }
        case 0x07: {
            UISControlInfo_t* ctrl = (UISControlInfo_t*)POP(stack);
            u32 id = POP(stack);
            UISUnloadRateFnc(info, ctrl, id);
            break;
        }
        case 0x0A: {  // plugin call; params stay on the stack
            u32 n = POP(stack);
            u32 plugin = POP(stack);
            u32 arg = POP(stack);
            void* object = (void*)POP(stack);
            UISParam_t* params = stack->sp - n;
            info->plugins[plugin](object, arg, n, params, params - 1);
            break;
        }
        case 0x76: {  // hint: id, then n params
            u32 n = POP(stack);
            stack->sp--;
            UISParam_t* p = stack->sp - n;
            g.w[0] = p[0].value;
            UISAddThreadAction(info, UIS_TA_HINT, &g, n, p + 1);
            break;
        }
        case 0x0B: {  // game message: message, then n params
            u32 n = POP(stack);
            stack->sp--;
            UISParam_t* p = stack->sp - n;
            info->gameMessage(p[0].value, screen->group, screen->id, n, p + 1, p - 1);
            break;
        }
        case 0x0C: {  // send event to
            u32 n = POP(stack);
            g.w[0] = POP(stack);
            g.w[1] = POP(stack);
            UISAddThreadAction(info, UIS_TA_SEND_EVENT_TO, &g, n, stack->sp - n);
            break;
        }
        case 0x0D:
            top[0] = (u32)screen->data;
            stack->sp++;
            break;
        case 0x0E: {  // strncpy
            UISString_t* dst = (UISString_t*)POP(stack);
            UISString_t* src = (UISString_t*)POP(stack);
            u32 n = POP(stack);
            if (dst->maxLength < n) {
                n = dst->maxLength;
            }
            if (src->maxLength < n) {
                n = src->maxLength;
            }
            strncpy(dst->text, src->text, n);
            break;
        }
        case 0x0F:  // push immediate
        case 0x10:
            READ_U32(stack, stack->sp->value);
            stack->sp++;
            break;
        case 0x11: {  // push float immediate
            u32 bits;
            READ_U32(stack, bits);
            *(f32*)&stack->sp->value = *(f32*)&bits;
            stack->sp++;
            break;
        }
        case 0x12:
            topi[-1] = (s32)topf[-1];
            break;
        case 0x13:
            topf[-1] = (f32)topi[-1];
            break;
        case 0x14:
            topi[-2] = (s32)topf[-2];
            break;
        case 0x15:
            topf[-2] = (f32)topi[-2];
            break;
        case 0x6A:
            break;
        case 0x18:  // load through pointer
            top[-1] = *(u32*)top[-1];
            break;
        case 0x19:  // store through pointer
            *(u32*)top[-1] = top[-2];
            stack->sp -= 2;
            break;
        case 0x69:  // address of local
            top[-1] = (u32)&top[topi[-1] - 1];
            break;
        case 0x1A:  // load local
            top[-1] = top[topi[-1] - 1];
            break;
        case 0x1B:  // store local
            top[topi[-1] - 1] = top[-2];
            stack->sp -= 2;
            break;
        case 0x1C: {
            u32 a = POP(stack);
            u32 b = POP(stack);
            PUSH(stack, a & b);
            break;
        }
        case 0x1D: {
            u32 a = POP(stack);
            u32 b = POP(stack);
            PUSH(stack, a | b);
            break;
        }
        case 0x1E:
            PUSH(stack, ~POP(stack));
            break;
        case 0x1F: {
            s32 a = POP(stack);
            s32 b = POP(stack);
            PUSH(stack, a != 0 && b != 0);
            break;
        }
        case 0x20: {
            s32 a = POP(stack);
            s32 b = POP(stack);
            PUSH(stack, a != 0 || b != 0);
            break;
        }
        case 0x21:
            PUSH(stack, POP(stack) == 0);
            break;
        case 0x22: {
            s32 v = topi[-1];
            topi[-1] = v < 0 ? -v : v;
            break;
        }
        case 0x23: {
            f32 v = POPF(stack);
            PUSHF(stack, v < 0.0f ? -v : v);
            break;
        }
        case 0x24:
            stack->sp[-1].value = -topi[-1];
            break;
        case 0x25:
            topi[-2] = topi[-2] + topi[-1];
            stack->sp--;
            break;
        case 0x26:
            topi[-2] = topi[-2] - topi[-1];
            stack->sp--;
            break;
        case 0x27:
            topi[-2] = topi[-2] * topi[-1];
            stack->sp--;
            break;
        case 0x28:
            topi[-2] = topi[-2] / topi[-1];
            stack->sp--;
            break;
        case 0x29:
            PUSHF(stack, -POPF(stack));
            break;
        case 0x2A:
            topf[-2] = topf[-2] + topf[-1];
            stack->sp--;
            break;
        case 0x2B:
            topf[-2] = topf[-2] - topf[-1];
            stack->sp--;
            break;
        case 0x2C:
            topf[-2] = topf[-2] * topf[-1];
            stack->sp--;
            break;
        case 0x2D:
            topf[-2] = topf[-2] / topf[-1];
            stack->sp--;
            break;
        case 0x2E:
            topi[-1]++;
            break;
        case 0x2F:
            topi[-1]--;
            break;
        case 0x30:
            topf[-1] = topf[-1] + 1.0f;
            break;
        case 0x31:
            topf[-1] = topf[-1] - 1.0f;
            break;
        case 0x32:
            topi[-2] = topi[-1] <= topi[-2];
            stack->sp--;
            break;
        case 0x33:
            topi[-2] = topi[-2] <= topi[-1];
            stack->sp--;
            break;
        case 0x34:
            topi[-2] = topi[-1] < topi[-2];
            stack->sp--;
            break;
        case 0x35:
            topi[-2] = topi[-2] < topi[-1];
            stack->sp--;
            break;
        case 0x36:
            topi[-2] = topi[-1] == topi[-2];
            stack->sp--;
            break;
        case 0x37:
            topi[-2] = topi[-1] != topi[-2];
            stack->sp--;
            break;
        case 0x38:
            topi[-2] = topf[-1] <= topf[-2];
            stack->sp--;
            break;
        case 0x39:
            topi[-2] = topf[-1] >= topf[-2];
            stack->sp--;
            break;
        case 0x3A:
            topi[-2] = topf[-1] < topf[-2];
            stack->sp--;
            break;
        case 0x3B:
            topi[-2] = topf[-1] > topf[-2];
            stack->sp--;
            break;
        case 0x3C:
            topi[-2] = topf[-1] == topf[-2];
            stack->sp--;
            break;
        case 0x3D:
            topi[-2] = topf[-1] != topf[-2];
            stack->sp--;
            break;
        case 0x3E: {  // branch if true
            u32 offset = POP(stack);
            if (POP(stack) != 0) {
                stack->pc = (u8*)stack->pc + offset;
            }
            break;
        }
        case 0x3F: {  // branch if false
            u32 offset = POP(stack);
            if (POP(stack) == 0) {
                stack->pc = (u8*)stack->pc + offset;
            }
            break;
        }
        case 0x40: {
            u32 offset = POP(stack);
            stack->pc = (u8*)stack->pc + offset;
            break;
        }
        case 0x41:  // dup
            top[0] = top[-1];
            stack->sp++;
            break;
        case 0x42:  // drop
            stack->sp--;
            break;
        case 0x43: {  // call
            u32 offset;
            READ_U32(stack, offset);
            PUSH(stack, (u32)stack->pc);
            stack->pc = (u8*)screen->data + offset;
            break;
        }
        case 0x44:  // return
            stack->pc = (void*)POP(stack);
            break;
        case 0x45: {  // load the screen named by a variable, if any
            u8 n = READ_U8(stack);
            u32* var = (u32*)POP(stack);
            u32 handle = *(u32*)((u8*)screen->data + *var);
            if (handle != 0xFFFFFFFF) {
                SET_SCREEN_GROUP(g, handle, screen);
                UISAddThreadAction(info, UIS_TA_LOAD_SCREEN, &g, n, stack->sp - n);
                while (n--) {
                    stack->sp--;
                }
            }
            break;
        }
        case 0x46: {  // deactivate a control list
            u32* var = (u32*)POP(stack);
            u32* list = (u32*)((u8*)screen->data + *var);
            s32 index = POP(stack);
            if (index < (s32)list[0]) {
                SET_CONTROL_GROUP(g, 0, (u32)list, (u32)((u8*)screen->data + list[1]), screen);
                UISAddThreadAction(info, UIS_TA_DEACTIVATE_CONTROL, &g, 0, NULL);
            }
            break;
        }
        case 0x09:  // activate the enabled sibling of this control
            SET_CONTROL_GROUP(g, 0, 0, (u32)UISFindSiblingEnableControl(screen, control), screen);
            UISAddThreadAction(info, UIS_TA_ACTIVATE_CONTROL, &g, 0, NULL);
            break;
        case 0x47: {  // activate control `index` of a list
            u32 id = POP(stack);
            u32* var = (u32*)POP(stack);
            UISParam_t param;
            s32 index = POP(stack);
            param.value = index;
            data = (u8*)screen->data;
            u32* list = (u32*)(*var + data);
            if (index < (s32)list[0]) {
                UISControlInfo_t* ctrl = (UISControlInfo_t*)(list[index + 2] + data);
                if (control != ctrl || ctrl->enabled == 0) {
                    SET_CONTROL_GROUP(g, id, (u32)list, (u32)ctrl, screen);
                    UISAddThreadAction(info, UIS_TA_ACTIVATE_CONTROL, &g, 1, &param);
                }
            }
            break;
        }
        case 0x48:
            g.h[0] = screen->parentGroup;
            g.h[1] = screen->parentId;
            UISAddThreadAction(info, UIS_TA_ACTIVATE_SCREEN, &g, 0, NULL);
            break;
        case 0x49: {  // string length
            UISString_t* str = (UISString_t*)POP(stack);
            if (str != NULL) {
                stack->sp[-1].value = str->maxLength;
            } else {
                stack->sp[-1].value = 0;
            }
            break;
        }
        case 0x4A: {  // get char
            s32 ch = 0;
            u32 index = POP(stack);
            UISString_t* str = (UISString_t*)POP(stack);
            if (str != NULL && index < str->maxLength) {
                ch = (s8)str->text[index];
            }
            stack->sp[-1].value = ch;
            break;
        }
        case 0x4B: {  // set char
            u32 ch = POP(stack);
            u32 index = POP(stack);
            UISString_t* str = (UISString_t*)POP(stack);
            if (str != NULL && index < str->maxLength) {
                str->text[index] = ch;
            }
            break;
        }
        case 0x4C: {  // sprintf
            u32 n = top[-1];
            stack->sp--;
            for (u32 i = 0; i < n; i++) {
                if (i < 20) {
                    uisFormatStringStack[n - i - 1].value = POP(stack);
                } else {
                    stack->sp--;
                }
            }
            UISString_t* fmt = (UISString_t*)POP(stack);
            UISString_t* dst = (UISString_t*)POP(stack);
            UISStringFormat(screen->data, dst, fmt, n, uisFormatStringStack);
            break;
        }
        case 0x4D: {  // send event
            UISParam_t param;
            param.value = POP(stack);
            g.w[0] = POP(stack);
            UISAddThreadAction(info, UIS_TA_SEND_EVENT, &g, 1, &param);
            break;
        }
        case 0x4E:
            stack->pc = (u8*)stack->pc + 4;
            break;
        case 0x52: {  // swap
            u32 t = top[-1];
            top[-1] = top[-2];
            stack->sp[-2].value = t;
            break;
        }
        case 0x4F:
            uisEventNotProcess = 0;
            break;
        case 0x50:
            uisEventNotProcess = 1;
            break;
        case 0x51:  // load a screen and wait for it
        case 0x70: {
            u8 n = READ_U8(stack);
            u32 index = info->numScreenRefs;
            if (index < info->maxScreenRefs) {
                UISParam_t* params = stack->sp - n;
                SET_SCREEN_GROUP(g, params[-1].value, screen);
                UISAddThreadAction(info, UIS_TA_LOAD_SCREEN, &g, n, params);
                while (n--) {
                    stack->sp--;
                }
                stack->sp--;
                UISScreenRef_t* ref = &info->screenRefs[index];
                ref->pc = stack->pc;
                ref->sp = stack->sp;
                ref->base = stack->base;
                ref->end = stack->end;
                ref->bottom = stack->bottom;
                ref->frame = frame;
                ref->stack = stack;
                stack->base = ref->sp;
                ref->group = g.h[0];
                ref->id = g.h[1];
                ref->screen = screen;
                ref->control = control;
                info->numScreenRefs++;
                return 3;
            }
            while (n--) {
                stack->sp--;
            }
            break;
        }
        case 0x54: {  // activate control
            u32 ctrl = POP(stack);
            SET_CONTROL_GROUP(g, 0, 0, ctrl, screen);
            UISAddThreadAction(info, UIS_TA_ACTIVATE_CONTROL, &g, 0, NULL);
            break;
        }
        case 0x55:  // push variable / its address
        case 0x6B: {
            u32* addr;
            READ_VAR_ADDR(stack, addr);
            if (op == 0x6B) {
                stack->sp->value = (u32)addr;
                stack->sp++;
            } else {
                stack->sp->value = *addr;
                stack->sp++;
            }
            break;
        }
        case 0x56: {  // store variable
            u32* addr;
            READ_VAR_ADDR(stack, addr);
            *addr = top[-1];
            stack->sp--;
            break;
        }
        case 0x57: {
            u32* addr;
            READ_VAR_ADDR(stack, addr);
            stack->sp->value = (u32)addr;
            stack->sp++;
            break;
        }
        case 0x5A:
            topi[-2] = topi[-2] % topi[-1];
            stack->sp--;
            break;
        case 0x5B:  // array element (0x6C-0x6E: its address)
        case 0x5D:
        case 0x5F:
        case 0x61:
        case 0x6C:
        case 0x6D:
        case 0x6E: {
            s32* array = NULL;
            int skip = 0;
            switch (op) {
            case 0x61:  // local holding the array pointer
                skip = 1;
                array = (s32*)top[topi[-1] - 1];
                break;
            case 0x5B:  // array in the locals
            case 0x6C:
                skip = 1;
                array = (s32*)&top[topi[-1] - 1];
                break;
            case 0x5D:  // array pointer on the stack
            case 0x6D:
                array = (s32*)topi[-1];
                skip = 1;
                break;
            case 0x5F:  // variable
            case 0x6E:
                skip = 0;
                READ_VAR_ADDR(stack, *(u32**)&array);
                break;
            }
            s32 numDims = array[0];
            u32* element;
            ARRAY_ELEMENT(top, array, skip, element);
            if (op == 0x6C || op == 0x6D || op == 0x6E) {
                top[-(numDims + skip)] = (u32)element;
            } else {
                top[-(numDims + skip)] = *element;
            }
            stack->sp -= numDims - (1 - skip);
            break;
        }
        case 0x5C:  // store array element
        case 0x5E:
        case 0x60:
        case 0x62: {
            s32* array = NULL;
            int skip = 0;
            switch (op) {
            case 0x62:
                skip = 1;
                array = (s32*)top[topi[-1] - 1];
                break;
            case 0x5C:
                skip = 1;
                array = (s32*)&top[topi[-1] - 1];
                break;
            case 0x5E:
                array = (s32*)topi[-1];
                skip = 1;
                break;
            case 0x60:
                skip = 0;
                READ_VAR_ADDR(stack, *(u32**)&array);
                break;
            }
            s32 numDims = array[0];
            u32* element;
            ARRAY_ELEMENT(top, array, skip + 1, element);
            *element = top[-1 - skip];
            stack->sp -= numDims + skip + 1;
            break;
        }
        case 0x63: {  // push n copies of the top
            s32 n;
            READ_U32(stack, n);
            for (s32 i = 0; i < n; i++) {
                top[i] = top[-1];
            }
            stack->sp += n;
            break;
        }
        case 0x64: {
            s32 n;
            READ_U32(stack, n);
            stack->sp -= n;
            break;
        }
        case 0x65: {  // duplicate the top n
            s32 n;
            READ_U32(stack, n);
            for (s32 i = 0; i < n; i++) {
                top[i] = top[i - n];
            }
            stack->sp += n;
            break;
        }
        case 0x66:  // fill an array
        case 0x67:
        case 0x68: {
            s32* array = NULL;
            s32 count = 1;
            int skip = 0;
            switch (op) {
            case 0x66:
                skip = 1;
                array = (s32*)&top[topi[-1] - 1];
                break;
            case 0x67:
                array = (s32*)topi[-1];
                skip = 1;
                break;
            case 0x68:
                skip = 0;
                READ_VAR_ADDR(stack, *(u32**)&array);
                break;
            }
            s32 numDims = array[0];
            for (s32 i = 1; i <= numDims; i++) {
                count *= array[i];
            }
            for (s32 i = 0; i < count; i++) {
                array[numDims + i + 1] = top[-1 - skip];
            }
            stack->sp -= skip + 1;
            break;
        }
        case 0x6F: {  // string table entry
            UISScrData_t* scr = screen->data;
            u32 index = top[-1];
            if (index < scr->numStrings) {
                top[-1] = (u32)&scr->strings[index];
            } else {
                top[-1] = 0;
            }
            break;
        }
        case 0x59: {  // visibility
            u32* addr;
            READ_VAR_ADDR(stack, addr);
            u8 mode = READ_U8(stack);
            UISUpdateVisibility(info, screen, mode, addr, top[-1]);
            stack->sp--;
            break;
        }
        case 0x73: {  // is a rate function running
            UISControlInfo_t* ctrl = (UISControlInfo_t*)POP(stack);
            u32 id = POP(stack);
            u32 found = UISFindRateFnc(info, ctrl, id);
            stack->sp->value = found < info->numRateFncs;
            stack->sp++;
            break;
        }
        case 0x72: {  // move screen draw position
            u32 delta = POP(stack);
            SET_SCREEN_GROUP(g, POP(stack), screen);
            g.w[3] = delta;
            UISAddThreadAction(info, UIS_TA_MOVE_SCREEN, &g, 0, NULL);
            break;
        }
        case 0x74: {  // set/clear script flag (bit < 0: all)
            u32 set = POP(stack);
            s32 bit = POP(stack);
            u32 mask = 0xFFFFFFFF;
            if (bit >= 0) {
                mask = 1 << bit;
            }
            if (set != 0) {
                info->scriptFlags |= mask;
            } else {
                info->scriptFlags &= ~mask;
            }
            break;
        }
        case 0x75: {
            u32 bit = POP(stack);
            PUSH(stack, info->scriptFlags & (1 << bit));
            break;
        }
        }
    }
    return 0;
}
