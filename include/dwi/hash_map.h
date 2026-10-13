// Dreamworks Interactive hash map (dwi::hash_map), reconstructed from its
// inlined uses. Open addressing over a prime-sized table; keys must
// never be the reserved values -1 (empty) or -2 (table boundary sentinel).
#ifndef _DWI_HASH_MAP_H_
#define _DWI_HASH_MAP_H_

#include "types.h"
#include "ea/real.h"
#include "game/memory.h"

namespace dwi {

static const u32 dwi_prime_list[27] = {
    0x00000035, 0x00000061, 0x000000C1, 0x00000185, 0x00000301, 0x00000607, 0x00000C07,
    0x00001807, 0x00003001, 0x00006011, 0x0000C005, 0x0001800D, 0x00030005, 0x00060019,
    0x000C0001, 0x00180005, 0x0030000B, 0x0060000D, 0x00C00005, 0x01800013, 0x03000005,
    0x06000017, 0x0C000013, 0x18000005, 0x30000059, 0x60000005, 0xC0000001,
};

template <class K, class V>
class hash_map {
public:
    struct entry {
        K key;
        V value;
    };

    hash_map() : m_table(NULL), m_capacity(0), m_count(0) {}

    ~hash_map() {
        MEM_free(m_table - 1);
        m_table = NULL;
        m_capacity = 0;
        m_count = 0;
    }

    // Allocate an empty table with room for at least `size` entries.
    void reserve(u32 size, const char* name = NULL) {
        const u32* prime;
        u32 capacity;

        for (prime = dwi_prime_list; prime != dwi_prime_list + 27; prime++) {
            if (*prime >= size) {
                break;
            }
        }
        capacity = (prime == dwi_prime_list + 27) ? prime[-1] : *prime;
        m_capacity = capacity;
        m_table = (entry*)DWI_allocalign(name, (capacity + 2) * sizeof(entry), 0x10, 0x400) + 1;
        for (entry* e = m_table; e != m_table + m_capacity; e++) {
            e->key = (K)-1;
        }
        m_table[-1].key = (K)-2;
        m_table[m_capacity].key = (K)-2;
        m_count = 0;
    }

    entry* end() { return m_table + m_capacity; }

    entry* find(K key) {
        entry* e;
        entry* last;

        if (m_count == 0) {
            return end();
        }
        last = m_table + m_capacity;
        e = m_table + (u32)key % m_capacity;
        for (;;) {
            if (e->key == (K)-1) {
                return last;
            }
            if (e->key == key) {
                return e;
            }
            if (++e == last) {
                e = m_table;
            }
        }
    }

    void insert(K key, V value) {
        entry* e = m_table + (u32)key % m_capacity;
        while (e->key != (K)-1) {
            if (++e == m_table + m_capacity) {
                e = m_table;
            }
        }
        e->key = key;
        e->value = value;
        m_count++;
    }

    entry* next(entry* e) {
        if (++e == m_table + m_capacity) {
            e = m_table;
        }
        return e;
    }

    // Removes `e`: entries of the same home slot are shifted back over the
    // hole, then the rest of the probe cluster is reinserted.
    void erase(entry* e) {
        if (e->key == (K)-1) {
            return;
        }
        u32 home = (u32)e->key % m_capacity;
        entry* hole = e;
        entry* from = next(e);
        for (;;) {
            bool used = from->key != (K)-1;
            while (used && (u32)from->key % m_capacity != home) {
                from = next(from);
                used = from->key != (K)-1;
            }
            *hole = *from;
            if (!used) {
                break;
            }
            hole = next(hole);
            from = next(from);
            while ((u32)hole->key % m_capacity != home) {
                hole = next(hole);
            }
        }
        m_count--;
        for (entry* p = next(hole); p->key != (K)-1; p = next(p)) {
            K key = p->key;
            V value = p->value;
            p->key = (K)-1;
            m_count--;
            insert(key, value);
        }
    }

    /* 0x0 */ entry* m_table;
    /* 0x4 */ u32 m_capacity;
    /* 0x8 */ u32 m_count;
};

}  // namespace dwi

#endif
