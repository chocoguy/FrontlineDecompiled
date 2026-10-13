// CPublisher: subject/observer subscriptions. An observer subscribes a
// pointer slot that points at a subject; when the subject is marked for
// destruction or destroyed, the observer's callback runs (usually clearing
// the slot). Subscriptions live in one global hash map keyed by subject.
// Hand-maintained.
#ifndef _GAME_PUBLISHER_H_
#define _GAME_PUBLISHER_H_

#include "types.h"
#include "game_types.h"
#include "game/destructor_queue.h"
#include "dwi/hash_map.h"

class CPublisher;
class IObserver;
class ISubject;

typedef void (*SubjectEventCallback)(IObserver&, ISubject**, ESubjectEvent);

class CPublisher {
public:
    // One subscription (the map's key is the subject).
    struct SItem {
        /* 0x0 */ IObserver* m_observer;
        /* 0x4 */ ISubject** m_slot;
        /* 0x8 */ SubjectEventCallback m_callback;
    };
    typedef dwi::hash_map<ISubject*, SItem> SubscriptionMap;

    static void PublishEvent(ISubject&, ESubjectEvent);  // 0x800D7718
    static void CancelSubscription(IObserver&, ISubject**);  // 0x800D7BB0
    static int AddSubscription(IObserver&, ISubject**, SubjectEventCallback);  // 0x800D7CB8
    static void RemoveObserver(IObserver&);  // 0x800D7D58
    static void Shutdown();  // 0x800D7E08
    static int Reset();  // 0x800D7E60
    static void Init(int);  // 0x800D7EB4
    static SubscriptionMap* sm_pThePublisher;  // 0x8034EC80
};

// size 0x4; vtable 0x8018A1D4
class ISubject : public IDestructible {
public:
    virtual void MarkForDestruction(int);  // 0x800D8148
    void NotifyObservers(ESubjectEvent event) { CPublisher::PublishEvent(*this, event); }  // 0x800D8180 (weak)
    virtual ~ISubject();  // 0x800D81A0
};

// size 0x4; vtable 0x8018A1C0
class IObserver : public ISubject {
public:
    virtual ~IObserver();  // 0x800D80E0
    // Points `slot` at `subject`, moving the subscription.
    template <class T>
    void SetSubject(T** slot, T* subject, SubjectEventCallback callback) {
        if (subject != *slot) {
            if (slot != NULL) {
                CPublisher::CancelSubscription(*this, (ISubject**)slot);
            }
            *slot = subject;
            if (subject != NULL) {
                CPublisher::SItem item;
                item.m_observer = this;
                item.m_slot = (ISubject**)slot;
                item.m_callback = callback;
                CPublisher::sm_pThePublisher->insert((ISubject*)*slot, item);
            }
        }
    }
};

// Clears the slot when its subject is marked for destruction.
void SECB_NullifyPointerOnMarked(IObserver&, ISubject**, ESubjectEvent);  // 0x800D80B0

#endif
