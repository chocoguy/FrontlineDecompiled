// CPublisher: subject/observer subscriptions.
#include "game/publisher.h"

CPublisher::SubscriptionMap* CPublisher::sm_pThePublisher;

// Observers are told about every subscription to the subject. On
// destruction each one is told once and dropped (like the original, this
// assumes each subscribed slot still points at the subject).
void CPublisher::PublishEvent(ISubject& subject, ESubjectEvent event) {
    SubscriptionMap* map = sm_pThePublisher;
    if (map->m_count == 0) {
        return;
    }
    if (event == SUBJECT_DESTROYED) {
        SubscriptionMap::entry* e;
        while ((e = sm_pThePublisher->find(&subject)) != sm_pThePublisher->end()) {
            if (&subject == *e->value.m_slot) {
                e->value.m_callback(*e->value.m_observer, e->value.m_slot, event);
                sm_pThePublisher->erase(e);
            }
        }
        return;
    }
    SubscriptionMap::entry* e = sm_pThePublisher->find(&subject);
    while (e != sm_pThePublisher->end()) {
        if (&subject == *e->value.m_slot) {
            e->value.m_callback(*e->value.m_observer, e->value.m_slot, event);
        }
        // The next subscription to the same subject.
        map = sm_pThePublisher;
        if (map->m_count == 0) {
            e = map->end();
        } else {
            ISubject* key = e->key;
            for (;;) {
                if (e->key == (ISubject*)-1) {
                    e = map->end();
                    break;
                }
                e = map->next(e);
                if (e->key == key) {
                    break;
                }
            }
        }
    }
}

void CPublisher::CancelSubscription(IObserver& observer, ISubject** slot) {
    SubscriptionMap* map = sm_pThePublisher;
    SubscriptionMap::entry* e;
    if (map->m_count == 0) {
        e = map->end();
    } else {
        SubscriptionMap::entry* last = map->m_table + map->m_capacity;
        e = map->m_table + (uptr)(*slot) % map->m_capacity;
        for (;;) {
            if (e->key == (ISubject*)-1) {
                e = last;
                break;
            }
            if (e->key == *slot && e->value.m_observer == &observer && e->value.m_slot == slot) {
                break;
            }
            if (++e == last) {
                e = map->m_table;
            }
        }
    }
    if (e != sm_pThePublisher->end()) {
        sm_pThePublisher->erase(e);
    }
}

int CPublisher::AddSubscription(IObserver& observer, ISubject** slot, SubjectEventCallback callback) {
    SItem item;
    item.m_observer = &observer;
    item.m_slot = slot;
    item.m_callback = callback;
    sm_pThePublisher->insert(*slot, item);
    return 1;
}

void CPublisher::RemoveObserver(IObserver& observer) {
    SubscriptionMap::entry* e = sm_pThePublisher->m_table;
    if (e->key == (ISubject*)-1 || e->key == (ISubject*)-2) {
        do {
            e++;
        } while (e->key == (ISubject*)-1);
    }
    while (e != sm_pThePublisher->end()) {
        if (e->value.m_observer == &observer) {
            sm_pThePublisher->erase(e);
        } else {
            do {
                e++;
            } while (e->key == (ISubject*)-1);
        }
    }
}

void CPublisher::Shutdown() {
    delete sm_pThePublisher;
}

int CPublisher::Reset() {
    SubscriptionMap* map = sm_pThePublisher;
    for (SubscriptionMap::entry* e = map->m_table; e != map->m_table + map->m_capacity; e++) {
        e->key = (ISubject*)-1;
    }
    map->m_table[-1].key = (ISubject*)-2;
    map->m_table[map->m_capacity].key = (ISubject*)-2;
    map->m_count = 0;
    return 0;
}

void CPublisher::Init(int size) {
    SubscriptionMap* map = new SubscriptionMap;
    if (map != NULL) {
        map->reserve(size + 1);
    }
    sm_pThePublisher = map;
}

void SECB_NullifyPointerOnMarked(IObserver& observer, ISubject** slot, ESubjectEvent event) {
    if (event == SUBJECT_MARKED) {
        observer.SetSubject(slot, (ISubject*)NULL, NULL);
    }
}

IObserver::~IObserver() {
    CPublisher::RemoveObserver(*this);
}

void ISubject::MarkForDestruction(int when) {
    IDestructible::MarkForDestruction(when);
    NotifyObservers(SUBJECT_MARKED);
}

ISubject::~ISubject() {
    NotifyObservers(SUBJECT_DESTROYED);
}
