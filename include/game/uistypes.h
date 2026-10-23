// Shared IStudio (UI tool) runtime structures. Hand-maintained.
#ifndef _GAME_UISTYPES_H_
#define _GAME_UISTYPES_H_

#include "types.h"
#include "game_types.h"

// A UI element's transform, as exported by IStudio.
struct UISTransform_t {
    /* 0x00 */ f32 pos[3];
    /* 0x0C */ f32 pivot[3];
    /* 0x18 */ f32 rot[3];    // degrees
    /* 0x24 */ f32 scale[3];
};

struct UISControlInfo_t;
struct UISScreen_t;

// A control's runtime values; UISGetActionPtrValue maps action IDs 16..39
// to the float fields.
struct UISControlInfo_t {
    /* 0x00 */ u32 visible;
    /* 0x04 */ u32 enabled;
    /* 0x08 */ UISTransform_t transform;
    /* 0x38 */ f32 field_0x38;
    /* 0x3C */ f32 colorMul[4];  // r, g, b, a
    /* 0x4C */ f32 colorAdd[4];
    /* 0x5C */ u32 field_0x5C;
    /* 0x60 */ u32 acceptsInput;  // gets input events while enabled
};

// UISEventEntry_t::flags
#define UISEVENT_EVENT 0x8000  // an event handler (id = event)
#define UISEVENT_HINT 0x4000   // a hint handler (id = hint)
// Otherwise a sub-control event: flags & 0x2FFF = sub-control, id = event.
// An entry with id 0xFFFF links a child control (pc = control index).
#define UISEVENT_CHILD 0xFFFF

struct UISEventEntry_t {
    /* 0x0 */ u16 flags;
    /* 0x2 */ u16 id;
    /* 0x4 */ u32 pc;
};

// An entry of a control list, or a screen-level plugin object.
#define UISLIST_CONTROL 0xFFFF  // type: data = index of a child control
struct UISListEntry_t {
    /* 0x0 */ u16 type;   // plugin type, or UISLIST_CONTROL
    /* 0x2 */ s16 state;  // plugin object: initialized / shown
    /* 0x4 */ u32 data;   // plugin object, or control index
};

// A group of linked controls and plugin objects, shown while *enable is
// non-zero.
struct UISControlList_t {
    /* 0x0 */ PTR32(u32) enable;
    /* 0x4 */ u32 numEntries;
    /* 0x8 */ PTR32(UISListEntry_t) entries;
};
FILE_SIZE_ASSERT(UISControlList_t, 0xC);

// size 0x14
struct UISControl_t {
    /* 0x00 */ PTR32(UISControlInfo_t) info;
    /* 0x04 */ u32 numLists;
    /* 0x08 */ PTR32(PTR32(UISControlList_t)) lists;
    /* 0x0C */ u32 numEvents;
    /* 0x10 */ PTR32(UISEventEntry_t) events;
};
FILE_SIZE_ASSERT(UISControl_t, 0x14);

// A loaded screen file.
struct UISScrData_t {
    /* 0x0 */ u32 numControls;
    /* 0x4 */ PTR32(UISControl_t) controls;
    /* 0x08 */ u32 numStrings;
    /* 0x0C */ PTR32(struct UISString_t) strings;
    /* 0x10 */ u32 numStringRefs;
    /* 0x14 */ PTR32(u32) stringRefs;  // offsets of words holding a string index
    /* 0x18 */ u32 numObjects;   // screen-level plugin objects
    /* 0x1C */ PTR32(struct UISListEntry_t) objects;
};
FILE_SIZE_ASSERT(UISScrData_t, 0x20);

// size 0xC
struct UISScreen_t {
    /* 0x0 */ u16 group;
    /* 0x2 */ u16 id;
    /* 0x4 */ u16 parentGroup;  // screen opcode 0x48 activates
    /* 0x6 */ u16 parentId;
    /* 0x8 */ UISScrData_t* data;
};

// One script parameter (a word: integer, pointer or float bits).
struct UISParam_t {
    /* 0x0 */ u32 value;
};

// A text buffer owned by a UI control.
struct UISString_t {
    /* 0x0 */ u32 unk_0x0;
    /* 0x4 */ u32 maxLength;
    /* 0x8 */ PTR32(char) text;
};
FILE_SIZE_ASSERT(UISString_t, 0xC);

// UISRateFnc_t::state
#define RATEFNC_NEW 0       // loaded this tick
#define RATEFNC_FINISHED 1  // removed by UISRemoveUnNessaryRateFncs
#define RATEFNC_RUNNING 2

// A property animation ("rate function") driving a control value (size 0x34).
struct UISRateFnc_t {
    /* 0x00 */ u32 id;
    /* 0x04 */ u8* data;
    /* 0x08 */ u32 ticks;       // elapsed
    /* 0x0C */ u32 ticksDone;   // handled in whole intervals
    /* 0x10 */ u32 interval;    // ticks per step (0: no stepping)
    /* 0x14 */ u32 state;  // 0 new, 1 finished (removed), 2 running
    /* 0x18 */ UISControlInfo_t* control;
    /* 0x1C */ UISScreen_t* screen;
    /* 0x20 */ u32 action;          // which control value (UISGetActionPtrValue)
    /* 0x24 */ f32 target;
    /* 0x28 */ f32 rate;            // change per tick
    /* 0x2C */ u8* data2;
    /* 0x30 */ UISControlInfo_t* control2;
};

// Arguments of a queued thread action; the layout depends on the action.
struct UISThreadGroupInfoT {
    union {
        u16 h[8];
        s16 sh[8];
        u32 w[4];
    };
};

// A queued thread action on the action stack (which grows downward). The
// record pointer points at `action`; params[numParams] lie just below
// numParams.
struct UISThreadActionRecord {
    /* -0x18 */ u32 numParams;
    /* -0x14 */ UISThreadGroupInfoT group;
    /* -0x04 */ u32 unused;
    /* 0x00 */ u32 action;
};

// The UI script interpreter's state.
struct UISStackInfo_t {
    /* 0x00 */ UISParam_t* bottom;
    /* 0x04 */ UISParam_t* base;  // bottom of the running thread's stack
    /* 0x08 */ UISParam_t* end;
    /* 0x0C */ UISParam_t* sp;  // grows upward
    /* 0x10 */ void* pc;
};

// A script thread suspended until the screen it loads is ready (size 0x28).
// The first 0x14 bytes are a copy of its UISStackInfo_t.
struct UISScreenRef_t {
    /* 0x00 */ UISParam_t* bottom;
    /* 0x04 */ UISParam_t* base;
    /* 0x08 */ UISParam_t* end;
    /* 0x0C */ UISParam_t* sp;
    /* 0x10 */ void* pc;
    /* 0x14 */ UISControlInfo_t* control;
    /* 0x18 */ UISScreen_t* screen;
    /* 0x1C */ UISParam_t* frame;
    /* 0x20 */ UISStackInfo_t* stack;
    /* 0x24 */ u16 id;     // the screen being loaded
    /* 0x26 */ u16 group;
};

// Game message handler (GameMessage) and plugin process functions.
typedef void (*UISGameMessageFnc)(unsigned long, unsigned short, unsigned short, unsigned long,
                                  UISParam_t*, UISParam_t*);
typedef void (*UISPluginProcessFnc)(void*, unsigned long, unsigned long, UISParam_t*,
                                    UISParam_t*);

typedef void* (*UISLoadScreenFnc)(unsigned short group, unsigned short screen);
typedef void (*UISUnloadScreenFnc)(unsigned short group, unsigned short screen, void* data);
typedef void (*UISTransformFnc)(UISTransformAction action, UISTransform_t* xf);

#define UIS_MAGIC 0x5549535F  // 'UIS_'

// UISInfo_t::flags
#define UISINFO_FLAG_INTERNAL_EVENTS 0x2  // running script events
#define UISINFO_FLAG_RATEFNCS 0x4         // running rate functions

// Script events (negative ids are internal).
#define UIS_EV_LOAD -2
#define UIS_EV_UNLOAD -3
#define UIS_EV_SCREEN_ACTIVATE -4
#define UIS_EV_SCREEN_DEACTIVATE -5
#define UIS_EV_CONTROL_ACTIVATE -6
#define UIS_EV_CONTROL_DEACTIVATE -7
#define UIS_EV_SEND -8
#define UIS_EV_INIT -9  // first load of the screen file
#define UIS_EV_IDLE -10
#define UIS_EV_DRAW -2  // plugin objects: draw
#define UIS_EV_SHOW -4  // plugin objects: visibility change

// UISUpdateVisibility / _ParseVisibility node kinds.
#define UISVIS_LIST 7
#define UISVIS_CONTROL 8
#define UISVIS_SCREEN 10

// IStudio runtime state, followed in memory by its arrays (see UISInit).
struct UISInfo_t {
    /* 0x00 */ u32 magic;  // UIS_MAGIC while initialized
    /* 0x04 */ u32 flags;
    /* 0x08 */ u32 ticksPerSecond;
    /* 0x0C */ UISGameMessageFnc gameMessage;
    /* 0x10 */ UISLoadScreenFnc loadScreen;
    /* 0x14 */ UISUnloadScreenFnc unloadScreen;
    /* 0x18 */ UISTransformFnc transform;
    /* 0x1C */ u32 field_0x1C;
    /* 0x20 */ u32 scriptFlags;  // set/tested by script opcodes 0x74/0x75
    /* 0x24 */ u32 activeScreen;  // index into screens, -1 for none
    /* 0x28 */ u32 maxScreens;
    /* 0x2C */ u32 numScreens;
    /* 0x30 */ UISScreen_t* screens;  // in draw order
    /* 0x34 */ u32 maxPlugins;
    /* 0x38 */ u32 numPlugins;
    /* 0x3C */ UISPluginProcessFnc* plugins;  // by plugin type
    /* 0x40 */ u32 maxRateFncs;
    /* 0x44 */ u32 numRateFncs;
    /* 0x48 */ UISRateFnc_t* rateFncs;
    /* 0x4C */ u32 maxScreenRefs;
    /* 0x50 */ u32 numScreenRefs;
    /* 0x54 */ UISScreenRef_t* screenRefs;
    /* 0x58 */ UISStackInfo_t stack;
    /* 0x6C */ UISStackInfo_t rateStack;  // for rate function scripts
    /* 0x80 */ u8 unk_0x80[0x20];
    // The action stack grows down from the top of the script stack.
    /* 0xA0 */ u32* actionStackBase;  // top of the (downward) action stack when empty
    /* 0xA4 */ u32* actionStackPtr;
    /* 0xA8 */ UISParam_t** stackBase;  // &stack.base
};

#endif
