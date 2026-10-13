#include "ea/snd/sndi.h"

// ARAM allocations, sorted by address, in 32-byte blocks.
struct SNDARAMENTRY {
    u32 start;
    s32 size;
};
struct SNDARAM {
    /* 0x0 */ u16 count;
    /* 0x2 */ u16 max;
    /* 0x4 */ u32 lo;  // pool, in blocks
    /* 0x8 */ u32 hi;
    /* 0xC */ SNDARAMENTRY* entries;
};
SNDARAM sndaram;

// Clips a gap to the pool.
static inline void clipgap(u32* start, s32* avail) {
    if (*start < sndaram.lo) {
        s32 d = sndaram.lo - *start;
        *start += d;
        *avail -= d;
    }
    if (*start + *avail > sndaram.hi) {
        *avail = sndaram.hi - *start;
    }
}

void SNDARAM_free(unsigned int addr) {
    SNDARAMENTRY* e = sndaram.entries;
    for (int i = 0; i < sndaram.count; i++, e++) {
        if (e->start == addr >> 5) {
            sndaram.count--;
            for (; i < sndaram.count; i++) {
                sndaram.entries[i] = sndaram.entries[i + 1];
            }
            return;
        }
    }
}

// First fit. Returns the ARAM address, or 0.
int SNDARAM_alloc(int size) {
    int i = 0;
    u32 start;
    s32 avail;

    if (sndaram.count >= sndaram.max) {
        return 0;
    }
    if (sndaram.count == 0) {
        start = sndaram.lo;
        avail = sndaram.hi - start;
        clipgap(&start, &avail);
        if ((size + 31) >> 5 > avail) {
            return 0;
        }
    } else {
        int blocks = (size + 31) >> 5;
        SNDARAMENTRY* e = sndaram.entries;
        for (i = 0; i < sndaram.count; i++, e++) {
            if (i == 0) {
                start = sndaram.lo;
                avail = e->start - sndaram.lo;
            } else {
                SNDARAMENTRY* prev = &sndaram.entries[i - 1];
                start = prev->start + prev->size;
                avail = e->start - start;
            }
            clipgap(&start, &avail);
            if (blocks <= avail) {
                for (int j = sndaram.count; j > i; j--) {
                    sndaram.entries[j] = sndaram.entries[j - 1];
                }
                goto found;
            }
        }
        SNDARAMENTRY* last = &sndaram.entries[i - 1];
        start = last->start + last->size;
        avail = sndaram.hi - start;
        clipgap(&start, &avail);
        if (blocks > avail) {
            return 0;
        }
    }
found:
    sndaram.entries[i].start = start;
    sndaram.entries[i].size = (size + 31) >> 5;
    sndaram.count++;
    return start << 5;
}

void SNDARAM_restore() {
    if (sndaram.entries != NULL) {
        SNDSYS_entercritical();
        SNDMEMI_free(sndaram.entries);
        SNDSYS_leavecritical();
        sndaram.entries = NULL;
    }
}

void SNDARAM_setpool(unsigned int base, int size) {
    sndaram.hi = (base + size) & ~4;
    if (base < 32) {
        base = 32;
    }
    sndaram.lo = base;
    if (base & 31) {
        sndaram.lo = (base + 31) & ~31;
    }
    sndaram.lo >>= 5;
    sndaram.hi >>= 5;
}

void SNDARAM_init(int max) {
    SNDSYS_entercritical();
    sndaram.entries = (SNDARAMENTRY*)SNDMEMI_allocz(max * 8);
    SNDSYS_leavecritical();
    sndaram.count = 0;
    sndaram.max = max;
}
