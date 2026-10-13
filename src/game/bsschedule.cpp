#include "game/bsschedule.h"

#include "game/bsobject.h"
#include "game/bstimer.h"
#include "game/bsutil.h"

#define BS_SCHEDULE_RECORDS 128

static BSScheduleRegistrationRecord_struct* g_AllRecords;
static BSScheduleRegistrationRecord_struct* g_FreeRecordList;

int BSScheduleGetMemoryRequirements() {
    return BS_SCHEDULE_RECORDS * sizeof(BSScheduleRegistrationRecord_struct);
}

void BSInitSchedule() {
    g_AllRecords = (BSScheduleRegistrationRecord_struct*)BSUtilGetMemory(
        BS_SCHEDULE_RECORDS * sizeof(BSScheduleRegistrationRecord_struct), 0);
    BSScheduleRegistrationRecord_struct* rec = g_AllRecords;
    for (int i = 0; i < BS_SCHEDULE_RECORDS - 1; i++, rec++) {
        rec->next = rec + 1;
        rec->prev = NULL;
    }
    rec->next = NULL;
    g_FreeRecordList = g_AllRecords;
}

void BSEndSchedule() {
    g_AllRecords = NULL;
    g_FreeRecordList = NULL;
}

BSSchedule::BSSchedule() {
    m_active = false;
}

BSSchedule::~BSSchedule() {}

void BSSchedule::Init(BSScheduleCallback callback, int maxPerUpdate, int period) {
    m_callback = callback;
    m_maxPerUpdate = maxPerUpdate;
    m_period = period;
    m_head = NULL;
    m_active = true;
    m_count = 0;
    m_tick = 0;
}

void BSSchedule::Reset(BSScheduleCallback callback, int maxPerUpdate, int period) {
    m_callback = callback;
    m_maxPerUpdate = maxPerUpdate;
    m_period = period;
    m_head = NULL;
    m_active = true;
    m_count = 0;
    m_tick = 0;
}

// Returns every record to the free list at once.
void BSSchedule::Shutdown() {
    m_active = false;
    if (m_head != NULL) {
        m_head->prev->next = g_FreeRecordList;
        g_FreeRecordList = m_head;
    }
    m_head = NULL;
}

inline BSScheduleRegistrationRecord_struct* BSSchedule::Find(BSObject* obj) {
    BSScheduleRegistrationRecord_struct* rec = m_head;
    for (int i = m_count; i > 0; i--) {
        if (rec->obj == obj) {
            return rec;
        }
        rec = rec->next;
    }
    return NULL;
}

inline void BSSchedule::Remove(BSScheduleRegistrationRecord_struct* rec) {
    if (m_count > 1) {
        rec->prev->next = rec->next;
        rec->next->prev = rec->prev;
        if (m_head == rec) {
            m_head = rec->next;
        }
    } else {
        m_head = NULL;
    }
    rec->next = g_FreeRecordList;
    rec->prev = NULL;
    g_FreeRecordList = rec;
    m_count--;
}

// Re-registering an object updates its existing record.
BSScheduleRegistrationRecord_struct* BSSchedule::Register(BSObject* obj, unsigned short event,
                                                          bool oneShot, int a, int b, int c) {
    bool existing = true;
    BSScheduleRegistrationRecord_struct* rec = Find(obj);
    if (rec == NULL) {
        rec = g_FreeRecordList;
        existing = false;
        g_FreeRecordList = rec->next;
    }
    rec->obj = obj;
    rec->event = event;
    rec->a = a;
    rec->b = b;
    rec->c = c;
    rec->objID = obj->m_uniqueID;
    rec->oneShot = oneShot;
    if (existing) {
        return rec;
    }
    // Insert before the head (at the end of the ring).
    rec->next = m_head;
    if (m_head != NULL) {
        rec->prev = m_head->prev;
        m_head->prev->next = rec;
        m_head->prev = rec;
    } else {
        m_head = rec;
        rec->next = rec;
        rec->prev = rec;
    }
    m_count++;
    return rec;
}

void BSSchedule::Unregister(BSScheduleRegistrationRecord_struct* rec) {
    Remove(rec);
}

void BSSchedule::Unregister(BSObject* obj) {
    BSScheduleRegistrationRecord_struct* rec = Find(obj);
    if (rec != NULL) {
        Remove(rec);
    }
}

// Every m_period ticks, polls up to m_maxPerUpdate records starting where the
// last poll stopped. Records whose object was destroyed are dropped.
void BSSchedule::Update() {
    if (++m_tick != m_period) {
        return;
    }
    m_tick = 0;
    if (m_head == NULL) {
        return;
    }
    BSScheduleRegistrationRecord_struct* rec = m_head;
    int polled = 0;
    while (polled < m_count) {
        if (rec->objID == rec->obj->m_uniqueID) {
            if (++polled > m_maxPerUpdate) {
                m_head = rec;
                return;
            }
            BSScheduleRegistrationRecord_struct* cur = rec;
            rec = rec->next;
            int delay = 0;
            int data;
            if (m_callback(cur->obj, cur->a, cur->b, cur->c, &data, &delay)) {
                if (delay != 0) {
                    BSRegisterTimerEvent(delay, cur->event, cur->obj, (void*)data, TIMER_KEEP_EARLIER);
                } else {
                    BSObjectTriggerEvent(cur->obj, cur->event, (void*)data, NULL, false);
                }
                if (cur->oneShot) {
                    Remove(cur);
                }
            }
        } else {
            BSScheduleRegistrationRecord_struct* cur = rec;
            rec = rec->next;
            Remove(cur);
        }
        if (m_count == 0) {
            return;
        }
    }
    m_head = rec;
}
