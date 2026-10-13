// IDestructible and the destructor queue: objects marked for destruction are
// destroyed (IDestructible::Destroy) a given number of frames later, at the
// end of a frame. Hand-maintained.
#ifndef _GAME_DESTRUCTOR_QUEUE_H_
#define _GAME_DESTRUCTOR_QUEUE_H_

#include "types.h"
#include "game_types.h"

class CDestructorQueue;
class IDestructible;

// size 0x4; vtable 0x8018A228
class IDestructible {
public:
    // vtable pointer at 0x0
    // Destroy after `when` more queue runs (0: at the end of this frame).
    // An object already queued is ignored.
    virtual void MarkForDestruction(int when);  // 0x800DD524
    virtual ~IDestructible();  // 0x8001AE00
    virtual void Destroy();  // 0x8001B054
};

// size 0xC
class CDestructorQueue {
public:
    // size 0x8
    struct SEntry {
        /* 0x0 */ IDestructible* m_object;
        /* 0x4 */ int m_when;  // runs left
    };

    /* 0x0 */ SEntry* m_entries;
    /* 0x4 */ int m_capacity;  // not checked
    /* 0x8 */ int m_count;

    // Destroys the objects that are due and keeps the rest (in order).
    static void Execute();  // 0x800DD3A4
    static void Shutdown();  // 0x800DD454
    // Forgets the queued objects without destroying them.
    static void Reset();  // 0x800DD4AC
    static void Init(int capacity);  // 0x800DD4BC
    static CDestructorQueue* sm_pSingleton;  // 0x8034EC90
};

#endif
