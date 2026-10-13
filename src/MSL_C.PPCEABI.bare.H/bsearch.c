#include <stdlib.h>

void* bsearch(const void* key, const void* table_base, size_t num_members, size_t member_size,
              int (*compare_members)(const void*, const void*)) {
    size_t l, r, m;
    int c;
    char* mp;

    if (!key || !table_base || !num_members || !member_size || !compare_members) {
        return NULL;
    }

    mp = (char*)table_base;
    c = compare_members(key, mp);

    if (c == 0) {
        return mp;
    }

    if (c < 0) {
        return NULL;
    }

    l = 1;
    r = num_members - 1;

    while (l <= r) {
        m = (l + r) / 2;
        mp = (char*)table_base + member_size * m;
        c = compare_members(key, mp);

        if (c == 0) {
            return mp;
        }

        if (c < 0) {
            r = m - 1;
        } else {
            l = m + 1;
        }
    }

    return NULL;
}
