// Dreamworks Interactive pooled list (dwi::list), reconstructed from its
// uses in alarm.cpp: a doubly linked list with a sentinel head whose nodes
// come from a fixed pool (free nodes chained through `next`).
#ifndef _DWI_LIST_H_
#define _DWI_LIST_H_

#include "types.h"
#include "ea/real.h"
#include "game/memory.h"

namespace dwi {

template <class T>
class list {
public:
    struct node {
        node* prev;
        node* next;
        T value;
    };

    list() : m_size(0), m_capacity(0), m_pool(NULL), m_free(NULL), m_ownsPool(true) {
        m_head.prev = &m_head;
        m_head.next = &m_head;
        m_head.value = T();
    }

    ~list() {
        clear();
        if (m_ownsPool) {
            MEM_free(m_pool);
        }
        m_pool = NULL;
    }

    node* begin() { return m_head.next; }
    node* end() { return &m_head; }
    int size() const { return m_size; }
    int capacity() const { return m_capacity; }

    void clear() {
        node* n = m_head.next;
        while (n != &m_head) {
            node* next = n->next;
            n->prev = NULL;
            n->next = m_free;
            m_free = n;
            n = next;
        }
        m_head.next = &m_head;
        m_head.prev = &m_head;
        m_size = 0;
    }

    // Moves the list into a new pool of `capacity` nodes.
    void reserve(int capacity, char* name) {
        if (capacity <= m_capacity) {
            return;
        }
        node* pool = (node*)DWI_allocalign(name, capacity * sizeof(node), 0x10, 0x400);
        node* next = NULL;
        for (node* n = &pool[capacity - 1]; n != pool; n--) {
            n->next = next;
            next = n;
        }
        pool->next = next;

        node* oldPool = m_pool;
        bool ownedOld = m_ownsPool;
        node* first = m_head.next;
        m_size = 0;
        m_capacity = capacity;
        m_pool = pool;
        m_free = pool;
        m_ownsPool = true;
        node* tail = &m_head;
        for (node* n = first; n != &m_head; n = n->next) {
            node* copy = m_free;
            m_free = copy->next;
            copy->prev = NULL;
            copy->next = NULL;
            copy->value = n->value;
            tail->next = copy;
            copy->prev = tail;
            tail = copy;
            m_size++;
        }
        tail->next = &m_head;
        m_head.prev = tail;
        if (ownedOld) {
            MEM_free(oldPool);
        }
    }

    // Takes a node from the pool; like the original, no check for an
    // exhausted pool.
    void push_back(const T& value) {
        node* n = m_free;
        m_free = n->next;
        if (n != NULL) {
            n->prev = NULL;
            n->next = NULL;
            n->value = value;
        }
        m_size++;
        node* last = m_head.prev;
        last->next = n;
        n->prev = last;
        n->next = &m_head;
        m_head.prev = n;
    }

    void erase(node* n) {
        n->prev->next = n->next;
        n->next->prev = n->prev;
        n->prev = NULL;
        n->next = m_free;
        m_free = n;
        m_size--;
    }

    /* 0x00 */ int m_size;
    /* 0x04 */ int m_capacity;
    /* 0x08 */ node* m_pool;
    /* 0x0C */ node* m_free;
    /* 0x10 */ node m_head;
    /* 0x20 */ bool m_ownsPool;
};

// A list position (passed by value).
template <class T>
class list_iter {
public:
    list_iter(typename list<T>::node* n) : m_node(n) {}
    typename list<T>::node* m_node;
};

}  // namespace dwi

#endif
