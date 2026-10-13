// Runtime structures of the BS ("behavior script") virtual machine that runs
// level scripts. Only fields decompiled code uses so far. Hand-maintained.
#ifndef _GAME_BSTYPES_H_
#define _GAME_BSTYPES_H_

#include "types.h"
#include "game_types.h"

class BSGO_Basic;
struct BSObject;

// A script vector (size 0xC).
struct BS_STRUCT_Vector_struct {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
};

// What a placed level object (trigger) was built from.
struct TriggerObjectData_struct {
    /* 0x00 */ u8 unk_0x0[0xC];
    /* 0x0C */ s32 m_type;  // object type (8 = mechanic path object, ...)
    /* 0x10 */ f32 m_position[3];
    /* 0x1C */ f32 m_orientation[4];  // a CQuaternion
    /* 0x2C */ u8 unk_0x2C[0x8];
    /* 0x34 */ u8 m_flags;  // bits 0 and 1 are read by scripts (GetObjInfo 0x42, 0x44)
    /* 0x35 */ u8 m_team;   // 0 neutral, 1 allied, 2 enemy
    /* 0x36 */ u8 unk_0x36[0x2];
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0x6];
    // Sorted group IDs: [0] = count, then (category << 16) | group.
    /* 0x44 */ u32* m_groups;
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;  // what CreateObject creates (pickups)
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;  // dump object type
    /* 0x72 */ u8 field_0x72;  // AI: CAIObject::field_0x214 (status var 2)
    /* 0x73 */ u8 m_aiSkill;  // AI: 0..2, aim error and turn rate
    // The rest depends on the object type.
    union {
        /* 0x74 */ s32 field_0x74;  // dump object id (types 2 and 8)
        struct {
            /* 0x74 */ u8 unk_0x74;
            /* 0x75 */ u8 m_aimTime;  // 0..2: 0.5, 1 or 2 seconds
            // AI properties set in the level editor
            // (SetAIObjectSharedDataPropVars); the s16 distances are 12.4
            // fixed point.
            /* 0x76 */ u8 field_0x76;
            /* 0x77 */ u8 field_0x77;
        } m_ai;  // (MWCC has no anonymous structs)
    };
    /* 0x78 */ u8 field_0x78;
    /* 0x79 */ u8 field_0x79;  // target search delay scale (DoTargetSearch)
    /* 0x7A */ u8 field_0x7A;
    /* 0x7B */ u8 unk_0x7B[0x3];
    /* 0x7E */ s16 field_0x7E;  // AI ranges in 12.4 fixed point: CAIObject::field_0x204
    /* 0x80 */ s16 field_0x80;  // sight range
    /* 0x82 */ s16 field_0x82;  // hearing radius
    /* 0x84 */ s16 field_0x84;
    /* 0x86 */ u8 unk_0x86[0x2];
    /* 0x88 */ s16 field_0x88;
    /* 0x8A */ s16 field_0x8A;
    /* 0x8C */ u8 unk_0x8C[0x4];
    /* 0x90 */ s32 field_0x90;  // dump object id (type 3)
    /* 0x94 */ u8 unk_0x94[0x4];
    /* 0x98 */ s32 field_0x98;
};

// A placed level object.
struct TriggerObject_struct {
    /* 0x0 */ u32 m_flags;  // bit 0: a mounted gun point in use (MarkMGAsUsed)
    /* 0x4 */ TriggerObjectData_struct* m_data;
    /* 0x8 */ BSObject* m_object;  // its script, if any
};

// Code addresses are (segment << 24) | word offset into
// BSCode_struct::m_code[segment].
#define BS_CODE_ADDRESS(code, addr) \
    ((u32*)(code)->m_code[(addr) >> 24] + ((addr) & 0xFFFFFF))

// BSCode_EventHandlerEntry_struct::m_flags
#define BSEVT_DECLARED 0x1   // the state handles the event (else it masks it)
#define BSEVT_ANY_STATE 0x2  // runs even if the thread changed state since
#define BSEVT_BARRIER 0x4    // stops the search of parent states

// An event handler of a state (size 8), sorted by event.
struct BSCode_EventHandlerEntry_struct {
    /* 0x0 */ s32 m_address;
    /* 0x4 */ u16 m_event;
    /* 0x6 */ u8 m_bucket;  // queue the event waits in (0..15, by priority)
    /* 0x7 */ u8 m_flags;   // BSEVT_*
};

// BSCode_MessageHandlerEntry_struct::m_filter: which senders the handler hears.
#define BSMSG_FILTER_ANY 0
#define BSMSG_FILTER_SENDER 1    // m_filterArg: 1 self, 2 own trigger's object, 3 trigger's object
#define BSMSG_FILTER_GROUP 2     // sender's trigger is in group (m_filterArg >> 16)
#define BSMSG_FILTER_CATEGORY 3  // sender and self share a group of category (m_filterArg >> 16)
#define BSMSG_FILTER_TARGET 4    // m_filterArg: 1 self is the target, 3 target's trigger is self

// BSCode_MessageHandlerEntry_struct::m_flags
#define BSMSG_GLOBAL 0x1     // registered in the message's list (can be sent to)
#define BSMSG_ANY_STATE 0x2  // runs even when sent from another state
#define BSMSG_BARRIER 0x4    // stops the search of older states' handlers

// A message handler of a state (size 0xC).
struct BSCode_MessageHandlerEntry_struct {
    /* 0x0 */ s32 m_address;
    /* 0x4 */ u32 m_filterArg;
    /* 0x8 */ u16 m_message;
    /* 0xA */ s8 m_filter;
    /* 0xB */ u8 m_flags;
};

// A state of a script thread: its event and message handlers (size 0x14).
struct BSHandler_struct {
    /* 0x00 */ s32 m_entry;  // code address run on entering the state
    /* 0x04 */ BSCode_EventHandlerEntry_struct* m_events;
    /* 0x08 */ BSCode_MessageHandlerEntry_struct* m_messages;
    /* 0x0C */ u16 m_parent;  // state whose handlers apply too (0xFFFF: none)
    /* 0x0E */ u8 m_level;  // nesting depth (substates are deeper)
    /* 0x0F */ u8 m_numMessages;
    /* 0x10 */ u8 m_numEvents;
};

// Handler slots that are not real handlers.
#define BS_HANDLER_NONE ((BSHandler_struct*)-1)
#define BS_HANDLER_INHERIT ((BSHandler_struct*)-2)  // use the base class's

// One script thread's code (size 0x1C).
struct BSCode_struct {
    /* 0x00 */ u32* m_code;  // m_numInherited inherited words, then the .cbs table
    /* 0x04 */ s32 field_0x4;
    /* 0x08 */ BSHandler_struct** m_handlers;
    /* 0x0C */ u16 m_numHandlers;
    /* 0x0E */ u8 unk_0xE[0x6];
    /* 0x14 */ u16 m_numLocals;  // words of local storage the thread needs
    /* 0x16 */ u16 field_0x16;
    /* 0x18 */ u8 m_numInherited;
};

// A compiled script class, loaded from <name>.sin (layout and strings) and
// <name>.cbs (code); lives inside the .sin file at offset 0x64.
struct BSClass_struct {
    /* 0x00 */ BSCode_struct* m_threads;
    /* 0x04 */ BSClass_struct* m_next;  // g_pcClassList
    /* 0x08 */ char m_name[0x40];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ void* m_sinFile;
    /* 0x50 */ void* m_cbsFile;
    /* 0x54 */ s32 m_memoryRequired;
    /* 0x58 */ u16 m_numThreads;
    /* 0x5A */ u8 m_loaded;
    /* 0x5B */ s8 m_release;  // -1 for scripts built for the release engine
    /* 0x5C */ s32 m_numStrings;
    /* 0x60 */ char** m_strings;
};

struct BSMessageRegistration_struct;

// A message handler a thread has registered, newest first (size 0x10).
struct BSMessageListEntry_struct {
    /* 0x0 */ BSMessageRegistration_struct* m_registration;
    /* 0x4 */ BSMessageRegistration_struct* m_replaced;
    /* 0x8 */ BSMessageListEntry_struct* m_next;
    /* 0xC */ u16 m_level;  // of the state that registered it
    /* 0xE */ u16 m_state;
};

// BSMachineThread_struct::m_flags
#define BSTHREAD_EXITING 0x1  // running the old state's exit handler (event 1)

// A running thread of a script object (size 0x20).
struct BSMachineThread_struct {
    /* 0x00 */ u32* m_frame;  // locals and arguments of the running function
    /* 0x04 */ u32* m_sp;  // points at the top word
    /* 0x08 */ u32* m_globals;
    /* 0x0C */ u32* m_pc;
    /* 0x10 */ BSHandler_struct* m_handler;
    /* 0x14 */ BSCode_struct* m_code;
    /* 0x18 */ BSMessageListEntry_struct* m_messages;
    /* 0x1C */ u16 m_state;
    /* 0x1E */ u16 m_flags;  // BSTHREAD_*
};

// A thread's message handler, in its message's list (size 0x18).
struct BSMessageRegistration_struct {
    /* 0x00 */ BSObject* m_object;
    /* 0x04 */ BSMachineThread_struct* m_thread;
    /* 0x08 */ BSCode_MessageHandlerEntry_struct* m_entry;
    /* 0x0C */ BSMessageRegistration_struct* m_prev;
    /* 0x10 */ BSMessageRegistration_struct* m_next;  // also the free list link
    /* 0x14 */ u16 m_state;
    /* 0x16 */ u16 m_flags;  // BSMSGREG_*
};

#define BSMSGREG_REPLACED 0x2

// A script instance attached to a game object (size 0x18).
struct BSObject {
    /* 0x00 */ BSClass_struct* m_class;
    // Instance memory: m_class->m_numThreads threads, then their locals.
    /* 0x04 */ BSMachineThread_struct* m_threads;
    /* 0x08 */ TriggerObject_struct* m_trigger;
    /* 0x0C */ BSGO_Basic* m_gameObject;
    /* 0x10 */ int m_uniqueID;  // -1 once destroyed
    // Set when event 0x2C (start) arrived before any thread could handle it;
    // until then, immediate events other than 0x2C are dropped.
    /* 0x14 */ bool m_started;
};


// Script event 0x2B (hit by bullet) data (size 0x18).
struct BS_STRUCT_HitByBulletInfo_struct {
    /* 0x00 */ BSObject* m_firedBy;  // the shooter's script, or NULL
    /* 0x04 */ int m_damage;
    /* 0x08 */ int m_side;  // 3 back, 2 front (along the right axis), else 0/1 by the forward axis
    /* 0x0C */ int m_bulletType;
    union {
        /* 0x10 */ const void* m_pPoly;  // the hit polygon (static objects only)
        /* 0x10 */ int m_location;  // explosions/impacts: hit location (9/10 in multiplayer, random elsewhere)
    };
    /* 0x14 */ int m_burstHit;  // soldiers: index of the hit within a burst (else 0)
};

#endif
