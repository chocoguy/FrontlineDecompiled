#include "game/bstimer.h"

#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bsutil.h"

#define TIMER_BUCKETS 120
#define TIMER_EVENTS 256

struct BSTimerEvent {
    /* 0x00 */ int time;  // whole seconds
    /* 0x04 */ u16 event;
    /* 0x08 */ BSObject* obj;
    /* 0x0C */ void* data;
    /* 0x10 */ bool refCounted;  // data is a reference-counted event block
    /* 0x14 */ BSTimerEvent* next;
};

struct BSTimerBucket {
    BSTimerEvent* head;
    BSTimerEvent* tail;
};

static BSTimerBucket* g_pTimerHashTable;
static BSTimerEvent* g_pTimerEventArray;
static BSTimerEvent* g_pFreeTimerEventHead;
static int g_LastHashIndex;
static float g_CurrentTime;
static u8* g_pBSTimerMemory;
static int g_pBSTimerMemoryOffset;

int BSTimerGetMemoryRequirements() {
    return TIMER_BUCKETS * sizeof(BSTimerBucket) + TIMER_EVENTS * sizeof(BSTimerEvent);
}

void BSInitTimer() {
    g_pBSTimerMemoryOffset = 0;
    g_pBSTimerMemory = (u8*)BSUtilGetMemory(
        TIMER_BUCKETS * sizeof(BSTimerBucket) + TIMER_EVENTS * sizeof(BSTimerEvent), 0);
    g_pTimerHashTable = (BSTimerBucket*)g_pBSTimerMemory;
    g_pBSTimerMemoryOffset = TIMER_BUCKETS * sizeof(BSTimerBucket);
    for (int i = 0; i < TIMER_BUCKETS; i++) {
        g_pTimerHashTable[i].head = NULL;
        g_pTimerHashTable[i].tail = NULL;
    }
    u8* events = g_pBSTimerMemory + g_pBSTimerMemoryOffset;
    g_pBSTimerMemoryOffset += TIMER_EVENTS * sizeof(BSTimerEvent);
    g_pTimerEventArray = (BSTimerEvent*)events;
    g_pFreeTimerEventHead = (BSTimerEvent*)events;
    int i;
    for (i = 0; i < TIMER_EVENTS - 1; i++) {
        g_pTimerEventArray[i].next = &g_pTimerEventArray[i + 1];
    }
    g_pTimerEventArray[TIMER_EVENTS - 1].next = NULL;
    g_LastHashIndex = 0;
    g_CurrentTime = 0.0f;
}

void BSEndTimer() {
    g_pTimerHashTable = NULL;
    g_pTimerEventArray = NULL;
    g_pFreeTimerEventHead = NULL;
}

static inline void FreeTimerEvent(BSTimerEvent* e) {
    e->next = g_pFreeTimerEventHead;
    g_pFreeTimerEventHead = e;
}

void BSUpdateTimer(float dt) {
    int from = (int)g_CurrentTime;
    g_CurrentTime += dt;
    int to = (int)g_CurrentTime;
    for (int t = from + 1; t <= to; t++) {
        if (++g_LastHashIndex == TIMER_BUCKETS) {
            g_LastHashIndex = 0;
        }
        BSTimerBucket* bucket = &g_pTimerHashTable[g_LastHashIndex];
        BSTimerEvent* prev = NULL;
        BSTimerEvent** link = &bucket->head;
        BSTimerEvent* next;
        for (BSTimerEvent* e = bucket->head; e != NULL; e = next) {
            next = e->next;
            if ((float)e->time <= g_CurrentTime) {
                BSObjectTriggerEvent(e->obj, e->event, e->data, NULL, false);
                if (e->refCounted) {
                    BSEventDecrimentReferenceCount(e->data);
                }
                *link = e->next;
                FreeTimerEvent(e);
            } else {
                prev = e;
                link = &e->next;
            }
        }
        g_pTimerHashTable[g_LastHashIndex].tail = prev;
    }
}

static inline void RemoveMatching(BSObject* obj, bool matchEvent, u16 event) {
    int index = g_LastHashIndex;
    int t = (int)g_CurrentTime + 1;
    int end = t + TIMER_BUCKETS;
    for (; t <= end; t++) {
        if (++index == TIMER_BUCKETS) {
            index = 0;
        }
        BSTimerBucket* bucket = &g_pTimerHashTable[index];
        BSTimerEvent* prev = NULL;
        BSTimerEvent** link = &bucket->head;
        BSTimerEvent* next;
        for (BSTimerEvent* e = bucket->head; e != NULL; e = next) {
            next = e->next;
            if (e->obj == obj && (!matchEvent || e->event == event)) {
                if (e->refCounted) {
                    BSEventDecrimentReferenceCount(e->data);
                }
                *link = e->next;
                FreeTimerEvent(e);
            } else {
                prev = e;
                link = &e->next;
            }
        }
        g_pTimerHashTable[index].tail = prev;
    }
}

void BSRegisterTimerEvent(int delay, unsigned short event, BSObject* obj, void* data,
                          ETimerReplaceMethod method) {
    switch (method) {
    case TIMER_REPLACE_ALL:
        RemoveMatching(obj, true, event);
        break;
    case TIMER_KEEP_EARLIER: {
        bool add = true;
        int fireTime = (int)(g_CurrentTime + (float)delay);
        int index = g_LastHashIndex;
        int t = (int)g_CurrentTime + 1;
        int end = t + TIMER_BUCKETS;
        for (; t <= end; t++) {
            if (++index == TIMER_BUCKETS) {
                index = 0;
            }
            BSTimerBucket* bucket = &g_pTimerHashTable[index];
            BSTimerEvent* prev = NULL;
            BSTimerEvent** link = &bucket->head;
            BSTimerEvent* next;
            for (BSTimerEvent* e = bucket->head; e != NULL; e = next) {
                next = e->next;
                if (e->obj == obj && e->event == event) {
                    if (e->time > fireTime) {
                        if (e->refCounted) {
                            BSEventDecrimentReferenceCount(e->data);
                        }
                        *link = e->next;
                        FreeTimerEvent(e);
                    } else {
                        // Note: the kept event doesn't advance prev/link, as in the original.
                        add = false;
                    }
                } else {
                    prev = e;
                    link = &e->next;
                }
            }
            g_pTimerHashTable[index].tail = prev;
        }
        if (!add) {
            if (g_pMemBlockAllocator.DoWeOwnThisMemory(data)) {
                BSEventDecrimentReferenceCount(data);
            }
            return;
        }
        break;
    }
    }

    bool refCounted = false;
    if (g_pMemBlockAllocator.DoWeOwnThisMemory(data)) {
        refCounted = true;
    }
    if (refCounted) {
        BSEventIncrimentReferenceCount(data);
    }
    if (obj->m_uniqueID == -1) {
        if (refCounted) {
            BSEventDecrimentReferenceCount(data);
        }
        return;
    }
    BSTimerEvent* e = g_pFreeTimerEventHead;
    g_pFreeTimerEventHead = e->next;
    e->event = event;
    e->obj = obj;
    e->time = (int)(g_CurrentTime + (float)delay);
    e->data = data;
    e->next = NULL;
    e->refCounted = refCounted;
    BSTimerBucket* bucket = &g_pTimerHashTable[e->time % TIMER_BUCKETS];
    if (bucket->tail != NULL) {
        bucket->tail->next = e;
    }
    bucket->tail = e;
    if (bucket->head == NULL) {
        bucket->head = e;
    }
}

void BSUnregisterTimerEvents(BSObject* obj) {
    RemoveMatching(obj, false, 0);
}
