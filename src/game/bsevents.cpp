// Script events: queued per priority bucket and run once per frame
// (BSEventProcessEvents), or queued as immediate events and run as soon as
// nothing else is being processed. Event data that scripts allocate comes
// from a reference-counted block allocator.
#include "game/bsevents.h"
#include "game/bsmachin.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/bsutil.h"

// A queued event (size 0x20).
struct BSEventRecord_struct {
    /* 0x00 */ BSObject* m_object;
    /* 0x04 */ BSMachineThread_struct* m_thread;
    /* 0x08 */ BSEventRecord_struct* m_next;  // also the free list link
    /* 0x0C */ BSCode_EventHandlerEntry_struct* m_entry;
    /* 0x10 */ void* m_data;
    /* 0x14 */ BSObject* m_sender;
    /* 0x18 */ int m_uniqueID;  // the object's, to drop events of destroyed objects
    /* 0x1C */ bool m_refCounted;  // m_data is an event memory block
    /* 0x1E */ u16 m_state;  // the state that declared the handler
};

// A queue of events (one per handler bucket).
struct BSEventBucket_struct {
    /* 0x0 */ BSEventRecord_struct* m_head;
    /* 0x4 */ BSEventRecord_struct* m_tail;
};

// An event data block: 0x20 bytes of data and a reference count.
struct BSEventMemoryBlock_struct {
    /* 0x00 */ u8 m_data[0x20];
    /* 0x20 */ int m_refCount;  // BSEVENT_BLOCK_FREE once freed
};

#define BSEVENT_BUCKETS 16
#define BSEVENT_RECORDS 128
#define BSEVENT_BLOCKS 128
#define BSEVENT_BLOCK_FREE -0x10000
#define BSEVENT_MEMORY (BSEVENT_BUCKETS * sizeof(BSEventBucket_struct) + BSEVENT_RECORDS * sizeof(BSEventRecord_struct))

// g_eProcessState
#define BSEVENT_IDLE 0
#define BSEVENT_PROCESSING 1
#define BSEVENT_PROCESSING_IMMEDIATE 2
#define BSEVENT_PROCESSING_MESSAGES 3

// Returned when the target object no longer exists.
#define BSEVENT_ERR_NO_OBJECT 0xE

static BSEventBucket_struct* g_pEventHashTable;
static BSEventRecord_struct* g_pEventRecordArray;
static BSEventRecord_struct* g_pFreeEventRecordHead;
static BSEventRecord_struct* g_pEventRecordExecutionListHead;
static BSEventRecord_struct* g_pEventRecordExecutionListTail;
static BSEventRecord_struct* g_pCurrentProcessingEvent;
static BSEventRecord_struct* g_pImmediateEventQueue;
static BSEventRecord_struct* g_pImmediateEventQueueTail;
static int g_eProcessState;
static u8* g_pBSEventMemory;
static int g_pBSEventMemoryOffset;

BSUtilObjectInstanceMemoryAllocator g_pMemBlockAllocator;

static inline void ReleaseBlock(BSEventMemoryBlock_struct* block) {
    int count = block->m_refCount;
    if (count <= 0 && count != BSEVENT_BLOCK_FREE) {
        block->m_refCount = BSEVENT_BLOCK_FREE;
        g_pMemBlockAllocator.FreeElement(block);
    }
}

static inline void DecrementBlock(void* data) {
    BSEventMemoryBlock_struct* block = (BSEventMemoryBlock_struct*)data;
    block->m_refCount--;
    ReleaseBlock(block);
}

// The handler of `event` in the thread's current state or its parents, and
// the state declaring it; a barrier or masking entry stops the search.
static inline u16 FindDeclaredState(u16 event, BSMachineThread_struct* thread,
                                    BSCode_EventHandlerEntry_struct** found) {
    PTR32(BSHandler_struct)* handlers = thread->m_code->m_handlers;
    u16 state = thread->m_state;
    while (state != 0xFFFF) {
        BSHandler_struct* handler = handlers[state];
        BSCode_EventHandlerEntry_struct* entry = handler->m_events;
        for (int i = handler->m_numEvents; i > 0; i--, entry++) {
            u8 flags = entry->m_flags;
            if (flags & BSEVT_BARRIER) {
                *found = NULL;
                return 0xFFFF;
            }
            if (entry->m_event == event) {
                if (flags & BSEVT_DECLARED) {
                    *found = entry;
                    return state;
                }
                *found = NULL;
                return 0xFFFF;
            }
            if (entry->m_event > event) {
                break;
            }
        }
        state = handler->m_parent;
    }
    *found = NULL;
    return 0xFFFF;
}

// Runs the handler of a queued event if its object still exists and the
// handler still applies, then releases the event data.
static inline void RunEvent(BSEventRecord_struct* rec) {
    BSObject* obj = rec->m_object;
    BSMachineThread_struct* thread = rec->m_thread;
    bool run = false;
    if (obj->m_uniqueID == rec->m_uniqueID) {
        u16 event = rec->m_entry->m_event;
        BSCode_EventHandlerEntry_struct* entry;
        u16 state = FindDeclaredState(event, thread, &entry);
        if (entry != NULL) {
            if (state != rec->m_state) {
                run = (entry->m_flags & BSEVT_ANY_STATE) != 0;
            } else {
                run = true;
            }
        }
        if (event == BSEVENT_START) {
            obj->m_started = true;
        }
        if (run) {
            s32 address = entry->m_address;
            thread->m_sp = g_piStackTop + 3;
            thread->m_frame = g_piStackTop + 2;
            thread->m_sp[-1] = (u32)PTR_TO_ADDR(rec->m_data);
            thread->m_sp[0] = (u32)PTR_TO_ADDR(rec->m_sender);
            thread->m_pc = BS_CODE_ADDRESS(thread->m_code, address);
            BSExecuteThread(rec->m_object, rec->m_thread);
        }
    }
    if (rec->m_refCounted) {
        DecrementBlock(rec->m_data);
    }
}

void BSEventCurrentlyProcessingMessages(bool processing) {
    g_eProcessState = processing ? BSEVENT_PROCESSING_MESSAGES : BSEVENT_IDLE;
}

bool BSEventIsEventMemoryBlock(void* data) {
    return g_pMemBlockAllocator.DoWeOwnThisMemory(data);
}

// Frees a block nobody holds a reference to.
void BSEventCheckMemBlockReferenceCount(void* data) {
    ReleaseBlock((BSEventMemoryBlock_struct*)data);
}

void* BSEventGetEventMemoryBlock(unsigned int) {
    BSEventMemoryBlock_struct* block = (BSEventMemoryBlock_struct*)g_pMemBlockAllocator.GetFreeElement(true);
    block->m_refCount = 0;
    return block;
}

unsigned short BSEventFindDeclaredState(unsigned short event, BSMachineThread_struct* thread,
                                        BSCode_EventHandlerEntry_struct** found) {
    return FindDeclaredState(event, thread, found);
}

// Runs up to `maxEvents` queued events, bucket by bucket; the rest wait for
// the next call. Immediate events are run after each one.
int BSEventProcessEvents(int maxEvents) {
    bool first = true;
    BSEventRecord_struct* last = NULL;
    g_eProcessState = BSEVENT_PROCESSING;
    for (int i = 0; i < BSEVENT_BUCKETS; i++) {
        BSEventBucket_struct* bucket = &g_pEventHashTable[i];
        if (bucket->m_head != NULL) {
            if (!first) {
                last->m_next = bucket->m_head;
                g_pEventRecordExecutionListTail = bucket->m_tail;
                last = bucket->m_tail;
            } else {
                // (The original leaves the list tail at its old value here.)
                if (g_pEventRecordExecutionListTail != NULL) {
                    g_pEventRecordExecutionListTail->m_next = bucket->m_head;
                } else {
                    g_pEventRecordExecutionListHead = bucket->m_head;
                }
                first = false;
                last = bucket->m_tail;
            }
        }
        bucket->m_head = NULL;
        bucket->m_tail = NULL;
    }

    BSEventRecord_struct* rec = g_pEventRecordExecutionListHead;
    BSEventRecord_struct* prev = NULL;
    for (int count = 0; rec != NULL && count < maxEvents; count++) {
        g_pCurrentProcessingEvent = rec;
        RunEvent(rec);
        prev = rec;
        rec = rec->m_next;
        g_eProcessState = BSEVENT_IDLE;
        BSEventProcessImmediateEvents();
        g_eProcessState = BSEVENT_PROCESSING;
    }
    if (prev != NULL) {
        prev->m_next = g_pFreeEventRecordHead;
        g_pFreeEventRecordHead = g_pEventRecordExecutionListHead;
    }
    g_pEventRecordExecutionListHead = rec;
    if (rec == NULL) {
        g_pEventRecordExecutionListTail = NULL;
    }
    g_eProcessState = BSEVENT_IDLE;
    return 0;
}

// Runs the immediate events (and any they queue) unless events are already
// being processed.
int BSEventProcessImmediateEvents() {
    if (g_eProcessState == BSEVENT_IDLE) {
        BSEventRecord_struct* rec = g_pImmediateEventQueue;
        g_eProcessState = BSEVENT_PROCESSING_IMMEDIATE;
        BSEventRecord_struct* prev = NULL;
        while (rec != NULL) {
            RunEvent(rec);
            prev = rec;
            rec = rec->m_next;
        }
        if (g_pImmediateEventQueue != NULL) {
            prev->m_next = g_pFreeEventRecordHead;
            g_pFreeEventRecordHead = g_pImmediateEventQueue;
        }
        g_pImmediateEventQueue = NULL;
        g_pImmediateEventQueueTail = NULL;
        g_eProcessState = BSEVENT_IDLE;
    }
    return 0;
}

// A new event record for `obj` (NULL if the object is gone, after releasing
// the data).
static inline BSEventRecord_struct* NewEventRecord(BSObject* obj, BSMachineThread_struct* thread,
                                                   BSCode_EventHandlerEntry_struct* entry, u16 state,
                                                   void* data, BSObject* sender) {
    bool refCounted = false;
    if (g_pMemBlockAllocator.DoWeOwnThisMemory(data)) {
        refCounted = true;
    }
    if (refCounted) {
        ((BSEventMemoryBlock_struct*)data)->m_refCount++;
    }
    if (obj->m_uniqueID == -1) {
        if (refCounted) {
            DecrementBlock(data);
        }
        return NULL;
    }
    BSEventRecord_struct* rec = g_pFreeEventRecordHead;
    g_pFreeEventRecordHead = rec->m_next;
    rec->m_object = obj;
    rec->m_thread = thread;
    rec->m_next = NULL;
    rec->m_state = state;
    rec->m_entry = entry;
    rec->m_data = data;
    rec->m_sender = sender;
    rec->m_uniqueID = obj->m_uniqueID;
    rec->m_refCounted = refCounted;
    return rec;
}

int BSEventQueueImmediateEvent(BSObject* obj, BSMachineThread_struct* thread,
                               BSCode_EventHandlerEntry_struct* entry, unsigned short state, void* data,
                               BSObject* sender) {
    BSEventRecord_struct* rec = NewEventRecord(obj, thread, entry, state, data, sender);
    if (rec == NULL) {
        return BSEVENT_ERR_NO_OBJECT;
    }
    if (g_pImmediateEventQueue != NULL) {
        g_pImmediateEventQueueTail->m_next = rec;
        g_pImmediateEventQueueTail = rec;
    } else {
        g_pImmediateEventQueue = rec;
        g_pImmediateEventQueueTail = rec;
    }
    return 0;
}

// Queues an event in its handler's bucket for the next BSEventProcessEvents.
int BSEventTriggerEvent(BSObject* obj, BSMachineThread_struct* thread, BSCode_EventHandlerEntry_struct* entry,
                        unsigned short state, void* data, BSObject* sender) {
    BSEventRecord_struct* rec = NewEventRecord(obj, thread, entry, state, data, sender);
    if (rec == NULL) {
        return BSEVENT_ERR_NO_OBJECT;
    }
    BSEventBucket_struct* bucket = &g_pEventHashTable[entry->m_bucket];
    if (bucket->m_tail != NULL) {
        bucket->m_tail->m_next = rec;
    }
    bucket->m_tail = rec;
    if (bucket->m_head == NULL) {
        bucket->m_head = rec;
    }
    return 0;
}

void BSEventDecrimentReferenceCount(void* data) {
    DecrementBlock(data);
}

void BSEventIncrimentReferenceCount(void* data) {
    ((BSEventMemoryBlock_struct*)data)->m_refCount++;
}

int BSEndEvent() {
    g_pEventHashTable = NULL;
    g_pEventRecordArray = NULL;
    g_pFreeEventRecordHead = NULL;
    return 0;
}

int BSInitEvent() {
    int blocks = g_pMemBlockAllocator.ComputeNeededSize(sizeof(BSEventMemoryBlock_struct), BSEVENT_BLOCKS, 0);
    g_pBSEventMemory = (u8*)BSUtilGetMemory(blocks + BSEVENT_MEMORY, 0);
    g_pBSEventMemoryOffset = 0;

    g_pEventHashTable = (BSEventBucket_struct*)g_pBSEventMemory;
    g_pBSEventMemoryOffset = BSEVENT_BUCKETS * sizeof(BSEventBucket_struct);
    int i;
    for (i = 0; i < BSEVENT_BUCKETS; i++) {
        g_pEventHashTable[i].m_head = NULL;
        g_pEventHashTable[i].m_tail = NULL;
    }

    g_pEventRecordArray = (BSEventRecord_struct*)(g_pBSEventMemory + g_pBSEventMemoryOffset);
    g_pBSEventMemoryOffset += BSEVENT_RECORDS * sizeof(BSEventRecord_struct);
    g_pFreeEventRecordHead = g_pEventRecordArray;
    for (i = 0; i < BSEVENT_RECORDS - 1; i++) {
        g_pEventRecordArray[i].m_next = &g_pEventRecordArray[i + 1];
    }
    g_pEventRecordArray[BSEVENT_RECORDS - 1].m_next = NULL;
    g_pEventRecordExecutionListHead = NULL;
    g_pEventRecordExecutionListTail = NULL;

    g_pMemBlockAllocator.Init(g_pBSEventMemory + g_pBSEventMemoryOffset, sizeof(BSEventMemoryBlock_struct),
                              BSEVENT_BLOCKS, 0);
    int size = g_pMemBlockAllocator.ComputeNeededSize(sizeof(BSEventMemoryBlock_struct), BSEVENT_BLOCKS, 0);
    g_pImmediateEventQueue = NULL;
    g_pBSEventMemoryOffset += size;
    g_eProcessState = BSEVENT_IDLE;
    return 0;
}

int BSEventGetMemoryRequirements() {
    return g_pMemBlockAllocator.ComputeNeededSize(sizeof(BSEventMemoryBlock_struct), BSEVENT_BLOCKS, 0) +
           BSEVENT_MEMORY;
}

// (Weak: emitted here for g_pMemBlockAllocator.)
BSUtilObjectInstanceMemoryAllocator::~BSUtilObjectInstanceMemoryAllocator() {}
