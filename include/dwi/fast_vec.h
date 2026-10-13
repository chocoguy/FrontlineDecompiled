// Dreamworks Interactive fast_vec, reconstructed from its out-of-line
// members (compartment, cdbobject): an array that can borrow an external
// buffer (usually on the stack) and only frees memory it owns. Nothing
// checks the capacity.
#ifndef _DWI_FAST_VEC_H_
#define _DWI_FAST_VEC_H_

#include "types.h"
#include "ea/real.h"
#include "game/memory.h"

namespace dwi {

template <class T>
class fast_vec {
public:
    fast_vec(int tag) : m_data(NULL), m_unk4(0), m_size(0), m_capacity(0), m_owned(true), m_tag(tag) {}

    ~fast_vec() {
        if (m_owned && m_data != NULL) {
            MEM_free(m_data);
        }
    }

    // Uses `buffer` (capacity elements) as the storage.
    void reserve(T* buffer, int capacity) {
        if (m_owned && m_data != NULL) {
            MEM_free(m_data);
        }
        m_data = buffer;
        m_capacity = capacity;
        m_owned = false;
    }

    // Allocates storage for `capacity` elements (16-aligned).
    void allocate(int capacity, const char* name = NULL) {
        if (m_owned && m_data != NULL) {
            MEM_free(m_data);
        }
        m_data = (T*)DWI_allocalign(name, capacity * sizeof(T), 0x10, 0x400);
        m_capacity = capacity;
        m_owned = true;
    }

    void push_back(T value) { m_data[m_size++] = value; }
    void pop_back() { m_size--; }
    T& back() { return m_data[m_size - 1]; }
    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }
    int size() const { return m_size; }
    int capacity() const { return m_capacity; }
    bool full() const { return m_size == m_capacity; }

    /* 0x00 */ T* m_data;
    /* 0x04 */ u32 m_unk4;
    /* 0x08 */ int m_size;
    /* 0x0C */ int m_capacity;
    /* 0x10 */ bool m_owned;
    /* 0x14 */ int m_tag;  // memory tag for growth
};

}  // namespace dwi

#endif
