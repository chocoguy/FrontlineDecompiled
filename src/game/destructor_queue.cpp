// The destructor queue: destroys marked objects at the end of a frame.
#include "game/destructor_queue.h"
#include "game/memory.h"
#include "ea/real.h"
#include "game/Moh2.h"

CDestructorQueue* CDestructorQueue::sm_pSingleton;

void CDestructorQueue::Execute() {
    SEntry* entries = sm_pSingleton->m_entries;
    SEntry* end = entries + sm_pSingleton->m_count;
    SEntry* kept = entries;
    for (SEntry* entry = entries; entry != end; entry++) {
        if (entry->m_when-- <= 0) {
            entry->m_object->Destroy();
            sm_pSingleton->m_count--;
        } else {
            *kept = *entry;
            kept++;
        }
    }
}

void CDestructorQueue::Shutdown() {
    CDestructorQueue* queue = sm_pSingleton;
    if (queue != NULL) {
        MEM_free(queue->m_entries);
        queue->m_entries = NULL;
        queue->m_capacity = 0;
        queue->m_count = 0;
        delete queue;
    }
    sm_pSingleton = NULL;
}

void CDestructorQueue::Reset() {
    sm_pSingleton->m_count = 0;
}

void CDestructorQueue::Init(int capacity) {
    CDestructorQueue* queue = new CDestructorQueue;
    if (queue != NULL) {
        queue->m_entries = (SEntry*)DWI_allocalign(NULL, capacity * sizeof(SEntry), 0x10, 0x400);
        queue->m_capacity = capacity;
        queue->m_count = 0;
    }
    sm_pSingleton = queue;
}

void IDestructible::MarkForDestruction(int when) {
    CDestructorQueue* queue = CDestructorQueue::sm_pSingleton;
    CDestructorQueue::SEntry* end = queue->m_entries + queue->m_count;
    for (CDestructorQueue::SEntry* entry = queue->m_entries; entry != end; entry++) {
        if (entry->m_object == this) {
            DebugMsg("Object already added to destructor queue. Ignoring duplicate request\n");
            return;
        }
    }
    CDestructorQueue::SEntry& entry = CDestructorQueue::sm_pSingleton->m_entries[queue->m_count++];
    entry.m_object = this;
    entry.m_when = when;
}
