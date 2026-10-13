#include <TRK_MINNOW_DOLPHIN/trk.h>

static void TRK_fill_mem(void* dst, int val, u32 n);

// These two live in .init so they work before the rest of the image is copied.
__declspec(section ".init") void* TRK_memset(void* dst, int val, u32 n) {
    TRK_fill_mem(dst, val, n);
    return dst;
}

__declspec(section ".init") void* TRK_memcpy(void* dst, const void* src, u32 n) {
    const u8* s;
    u8* d;

    s = (const u8*)src - 1;
    d = (u8*)dst - 1;
    n++;
    while (--n != 0) {
        *++d = *++s;
    }
    return dst;
}

// MSL __fill_mem: byte fill up to alignment, then 32-byte and word blocks.
static void TRK_fill_mem(void* dst, int val, u32 n) {
    u32 v;
    u32 i;
    u8* cdst;
    u32* ldst;

    v = (u8)val;
    cdst = (u8*)dst - 1;

    if (n >= 32) {
        i = (~(u32)cdst) & 3;
        if (i) {
            n -= i;
            do {
                *++cdst = v;
            } while (--i);
        }

        if (v) {
            v |= v << 24 | v << 16 | v << 8;
        }

        ldst = (u32*)(cdst + 1) - 1;

        i = n >> 5;
        if (i) {
            do {
                *++ldst = v;
                *++ldst = v;
                *++ldst = v;
                *++ldst = v;
                *++ldst = v;
                *++ldst = v;
                *++ldst = v;
                *++ldst = v;
            } while (--i);
        }

        i = (n & 31) >> 2;
        if (i) {
            do {
                *++ldst = v;
            } while (--i);
        }

        cdst = (u8*)(ldst + 1) - 1;
        n &= 3;
    }

    if (n) {
        do {
            *++cdst = v;
        } while (--n);
    }
}
