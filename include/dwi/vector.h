// Dreamworks Interactive vector (dwi::vector), reconstructed from its uses
// in buddy.cpp: size, capacity, a DWI_allocalign'd array and whether the
// vector owns it. Elements are plain data; nothing checks the capacity.
#ifndef _DWI_VECTOR_H_
#define _DWI_VECTOR_H_

#include "types.h"
#include "ea/real.h"
#include "game/memory.h"

namespace dwi {

template <class T>
class vector {
public:
    vector() : m_size(0), m_capacity(0), m_data(NULL), m_ownsData(true) {}

    ~vector() {
        clear();
        if (m_ownsData) {
            MEM_free(m_data);
        }
    }

    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }
    int size() const { return m_size; }
    T& operator[](int i) { return m_data[i]; }

    void clear() { m_size = 0; }

    void push_back(const T& value) {
        T* slot = end();
        if (slot != NULL) {
            *slot = value;
        }
        m_size++;
    }

    // Moves the elements into a new array of `capacity`.
    void reserve(int capacity, char* name) {
        if (capacity <= m_capacity) {
            return;
        }
        T* data = (T*)DWI_allocalign(name, capacity * sizeof(T), 0x10, 0x400);
        T* dst = data;
        for (T* src = begin(); src != end(); src++, dst++) {
            *dst = *src;
        }
        clear();
        if (m_ownsData) {
            MEM_free(m_data);
        }
        m_data = data;
        m_capacity = capacity;
        m_ownsData = true;
    }

    /* 0x0 */ int m_size;
    /* 0x4 */ int m_capacity;
    /* 0x8 */ T* m_data;
    /* 0xC */ bool m_ownsData;
};

}  // namespace dwi

#endif
