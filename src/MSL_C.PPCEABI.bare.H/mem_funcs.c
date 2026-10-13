#include <string.h>

// Block copies for memmove: 32 bytes per iteration once dst is word-aligned;
// the unaligned variants merge two source words with shifts.

void __copy_longs_aligned(void* dst, const void* src, size_t n) {
    const unsigned char* cps;
    unsigned char* cpd;
    const unsigned long* lps;
    unsigned long* lpd;
    unsigned long i;

    i = (-(unsigned long)dst) & 3;

    cps = ((const unsigned char*)src) - 1;
    cpd = ((unsigned char*)dst) - 1;

    if (i) {
        n -= i;
        do {
            *++cpd = *++cps;
        } while (--i);
    }

    lps = ((const unsigned long*)(cps + 1)) - 1;
    lpd = ((unsigned long*)(cpd + 1)) - 1;

    i = n >> 5;
    if (i) {
        do {
            *++lpd = *++lps;
            *++lpd = *++lps;
            *++lpd = *++lps;
            *++lpd = *++lps;
            *++lpd = *++lps;
            *++lpd = *++lps;
            *++lpd = *++lps;
            *++lpd = *++lps;
        } while (--i);
    }

    i = (n & 31) >> 2;
    if (i) {
        do {
            *++lpd = *++lps;
        } while (--i);
    }

    cps = ((const unsigned char*)(lps + 1)) - 1;
    cpd = ((unsigned char*)(lpd + 1)) - 1;

    n &= 3;
    if (n) {
        do {
            *++cpd = *++cps;
        } while (--n);
    }
}

void __copy_longs_rev_aligned(void* dst, const void* src, size_t n) {
    const unsigned char* cps;
    unsigned char* cpd;
    const unsigned long* lps;
    unsigned long* lpd;
    unsigned long i;

    cps = ((const unsigned char*)src) + n;
    cpd = ((unsigned char*)dst) + n;

    i = ((unsigned long)cpd) & 3;

    if (i) {
        n -= i;
        do {
            *--cpd = *--cps;
        } while (--i);
    }

    lps = (const unsigned long*)cps;
    lpd = (unsigned long*)cpd;

    i = n >> 5;
    if (i) {
        do {
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
        } while (--i);
    }

    i = (n & 31) >> 2;
    if (i) {
        do {
            *--lpd = *--lps;
        } while (--i);
    }

    cps = (const unsigned char*)lps;
    cpd = (unsigned char*)lpd;

    n &= 3;
    if (n) {
        do {
            *--cpd = *--cps;
        } while (--n);
    }
}

void __copy_longs_unaligned(void* dst, const void* src, size_t n) {
    const unsigned char* cps;
    unsigned char* cpd;
    const unsigned long* lps;
    unsigned long* lpd;
    unsigned long i, v1, v2;
    unsigned int src_offset, left_shift, right_shift;

    i = (-(unsigned long)dst) & 3;

    cps = ((const unsigned char*)src) - 1;
    cpd = ((unsigned char*)dst) - 1;

    if (i) {
        n -= i;
        do {
            *++cpd = *++cps;
        } while (--i);
    }

    src_offset = ((unsigned int)(cps + 1)) & 3;

    left_shift = src_offset << 3;
    right_shift = 32 - left_shift;

    cps -= src_offset;

    lps = ((const unsigned long*)(cps + 1)) - 1;
    lpd = ((unsigned long*)(cpd + 1)) - 1;

    i = n >> 3;

    v1 = *++lps;

    do {
        v2 = *++lps;
        *++lpd = (v1 << left_shift) | (v2 >> right_shift);
        v1 = *++lps;
        *++lpd = (v2 << left_shift) | (v1 >> right_shift);
    } while (--i);

    if (n & 4) {
        v2 = *++lps;
        *++lpd = (v1 << left_shift) | (v2 >> right_shift);
    }

    cps = ((const unsigned char*)(lps + 1)) - 1;
    cpd = ((unsigned char*)(lpd + 1)) - 1;

    n &= 3;
    if (n) {
        cps -= 4 - src_offset;
        do {
            *++cpd = *++cps;
        } while (--n);
    }
}

void __copy_longs_rev_unaligned(void* dst, const void* src, size_t n) {
    const unsigned char* cps;
    unsigned char* cpd;
    const unsigned long* lps;
    unsigned long* lpd;
    unsigned long i, v1, v2;
    unsigned int src_offset, left_shift, right_shift;

    cps = ((const unsigned char*)src) + n;
    cpd = ((unsigned char*)dst) + n;

    i = ((unsigned long)cpd) & 3;

    if (i) {
        n -= i;
        do {
            *--cpd = *--cps;
        } while (--i);
    }

    src_offset = ((unsigned int)cps) & 3;

    left_shift = src_offset << 3;
    right_shift = 32 - left_shift;

    cps += 4 - src_offset;

    lps = (const unsigned long*)cps;
    lpd = (unsigned long*)cpd;

    i = n >> 3;

    v1 = *--lps;

    do {
        v2 = *--lps;
        *--lpd = (v2 << left_shift) | (v1 >> right_shift);
        v1 = *--lps;
        *--lpd = (v1 << left_shift) | (v2 >> right_shift);
    } while (--i);

    if (n & 4) {
        v2 = *--lps;
        *--lpd = (v2 << left_shift) | (v1 >> right_shift);
    }

    cps = (const unsigned char*)lps;
    cpd = (unsigned char*)lpd;

    n &= 3;
    if (n) {
        cps += src_offset;
        do {
            *--cpd = *--cps;
        } while (--n);
    }
}
