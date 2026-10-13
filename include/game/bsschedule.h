// BSSchedule: polls registered script objects every few ticks and raises an
// event (or a timer) on them when a condition callback fires.
// Hand-maintained.
#ifndef _GAME_BSSCHEDULE_H_
#define _GAME_BSSCHEDULE_H_

#include "types.h"
#include "game/bstypes.h"

// Returns true to fire: *data is the event data, *delay a timer delay (0 =
// raise the event now).
typedef bool (*BSScheduleCallback)(BSObject* obj, int a, int b, int c, int* data, int* delay);

// size 0x20; records form a circular doubly linked list
struct BSScheduleRegistrationRecord_struct {
    /* 0x00 */ BSObject* obj;
    /* 0x04 */ int objID;  // obj->m_uniqueID when registered; stale when they differ
    /* 0x08 */ int a;
    /* 0x0C */ int b;
    /* 0x10 */ int c;
    /* 0x14 */ BSScheduleRegistrationRecord_struct* next;
    /* 0x18 */ BSScheduleRegistrationRecord_struct* prev;
    /* 0x1C */ bool oneShot;
    /* 0x1E */ u16 event;
};

class BSSchedule {
public:
    /* 0x00 */ bool m_active;
    /* 0x04 */ int m_count;
    /* 0x08 */ BSScheduleRegistrationRecord_struct* m_head;  // next to poll
    /* 0x0C */ BSScheduleCallback m_callback;
    /* 0x10 */ int m_tick;
    /* 0x14 */ int m_period;          // ticks between polls
    /* 0x18 */ int m_maxPerUpdate;    // records polled per poll

    BSSchedule();  // 0x8003ACE8
    ~BSSchedule();  // 0x8003ACAC
    void Init(BSScheduleCallback callback, int maxPerUpdate, int period);  // 0x8003AC84
    void Reset(BSScheduleCallback callback, int maxPerUpdate, int period);  // 0x8003AC5C
    void Shutdown();  // 0x8003AC2C
    BSScheduleRegistrationRecord_struct* Register(BSObject* obj, unsigned short event, bool oneShot,
                                                  int a, int b, int c);  // 0x8003AB5C
    void Unregister(BSScheduleRegistrationRecord_struct* rec);  // 0x8003AAF4
    void Unregister(BSObject* obj);  // 0x8003AA58
    void Update();  // 0x8003A844

private:
    BSScheduleRegistrationRecord_struct* Find(BSObject* obj);
    void Remove(BSScheduleRegistrationRecord_struct* rec);
};

int BSScheduleGetMemoryRequirements();  // 0x8003AD60
void BSInitSchedule();  // 0x8003AD04
void BSEndSchedule();  // 0x8003ACF4

#endif
