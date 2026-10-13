// memset/memcpy live in .init: __start uses them before the rest of the image
// is in place.

typedef unsigned long size_t;

void __fill_mem(void* dst, int val, unsigned long n);

__declspec(section ".init") void* memset(void* dst, int val, size_t n) {
    __fill_mem(dst, val, n);
    return dst;
}

__declspec(section ".init") void __fill_mem(void* dst, int val, unsigned long n) {
    unsigned long v = (unsigned char)val;
    unsigned long i;
    unsigned char* cpd = ((unsigned char*)dst) - 1;
    unsigned long* lpd;

    if (n >= 32) {
        i = (~(unsigned long)cpd) & 3;

        if (i) {
            n -= i;
            do {
                *++cpd = v;
            } while (--i);
        }

        if (v) {
            v |= v << 24 | v << 16 | v << 8;
        }

        lpd = ((unsigned long*)(cpd + 1)) - 1;

        i = n >> 5;
        if (i) {
            do {
                *++lpd = v;
                *++lpd = v;
                *++lpd = v;
                *++lpd = v;
                *++lpd = v;
                *++lpd = v;
                *++lpd = v;
                *++lpd = v;
            } while (--i);
        }

        i = (n & 31) >> 2;
        if (i) {
            do {
                *++lpd = v;
            } while (--i);
        }

        cpd = ((unsigned char*)(lpd + 1)) - 1;
        n &= 3;
    }

    if (n) {
        do {
            *++cpd = v;
        } while (--n);
    }
}

__declspec(section ".init") void* memcpy(void* dst, const void* src, size_t n) {
    const unsigned char* p;
    unsigned char* q;
    int rev = ((unsigned long)src < (unsigned long)dst);

    if (!rev) {
        for (p = (const unsigned char*)src - 1, q = (unsigned char*)dst - 1, n++; --n;) {
            *++q = *++p;
        }
    } else {
        for (p = (const unsigned char*)src + n, q = (unsigned char*)dst + n, n++; --n;) {
            *--q = *--p;
        }
    }

    return dst;
}
