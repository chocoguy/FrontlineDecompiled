// Dreamworks Interactive object pool (dwi::CFactory<T, P>), reconstructed
// from collision.cpp: a fixed array of T (its own allocation or a caller's
// buffer) with free elements chained through their first word.
#ifndef _DWI_FACTORY_H_
#define _DWI_FACTORY_H_

#include "types.h"
#include "ea/real.h"
#include "game/memory.h"

namespace dwi {

template <class T, class P>
class CFactory {
public:
    CFactory(const char* name, int capacity, T* buffer) {
        Init(name, capacity, buffer);
    }

    ~CFactory() {
        if (m_ownsPool) {
            MEM_free(m_pool);
        }
        m_pool = NULL;
        m_free = NULL;
        m_capacity = 0;
        m_used = 0;
    }

    void Init(const char* name, int capacity, T* buffer) {
        if (m_ownsPool) {
            MEM_free(m_pool);
        }
        T* pool = buffer != NULL ? buffer
                                 : (T*)DWI_allocalign(name, capacity * sizeof(T), 0x10, 0x400);
        m_free = NULL;
        m_used = 0;
        m_pool = pool;
        m_capacity = capacity;
        m_ownsPool = buffer == NULL;
        m_free = NULL;
        m_used = 0;
        for (int i = capacity - 1; i >= 0; i--) {
            *(T**)&m_pool[i] = m_free;
            m_free = &m_pool[i];
        }
    }

    // No check for an empty pool, as in the original.
    T* Alloc() {
        T* p = m_free;
        m_free = *(T**)p;
        m_used++;
        return p;
    }

    void Free(T* p) {
        m_used--;
        *(T**)p = m_free;
        m_free = p;
    }

    /* 0x00 */ T* m_pool;
    /* 0x04 */ T* m_free;
    /* 0x08 */ int m_capacity;
    /* 0x0C */ int m_used;
    /* 0x10 */ bool m_ownsPool;
};

}  // namespace dwi

#endif
