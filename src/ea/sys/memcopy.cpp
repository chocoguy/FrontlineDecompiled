#include "ea/sys/mem.h"

// Copy forwards with the widest moves the relative alignment of the two pointers allows.
extern "C" void MEM_copy(void* dest, const void* src, int size) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    int diff;

    if (size == 0) {
        return;
    }
    diff = s - d;
    if (diff == 0) {
        return;
    }

    if (diff & 1) {
        // Bytes only.
        if (size >= 4) {
            u32 n = (u32)size >> 2;
            u32 i;
            for (i = n; i != 0; i--) {
                u8 b1 = s[1];
                u8 b2 = s[2];
                u8 b3 = s[3];
                u8 b0 = s[0];
                d[0] = b0;
                d[1] = b1;
                d[2] = b2;
                d[3] = b3;
                s += 4;
                d += 4;
            }
            size -= n << 2;
        }
        if (size > 0) {
            for (; size != 0; size--) {
                *d++ = *s++;
            }
        }
    } else if (diff & 2) {
        // Halfwords.
        if (((uptr)(s) & 1) && size >= 1) {
            *d++ = *s++;
            size--;
        }
        if (size >= 8) {
            u32 n = (u32)size >> 3;
            u32 i;
            for (i = n; i != 0; i--) {
                s16 h1 = ((const s16*)s)[1];
                s16 h2 = ((const s16*)s)[2];
                s16 h3 = ((const s16*)s)[3];
                s16 h0 = ((const s16*)s)[0];
                s += 8;
                ((s16*)d)[0] = h0;
                ((s16*)d)[1] = h1;
                ((s16*)d)[2] = h2;
                ((s16*)d)[3] = h3;
                d += 8;
            }
            size -= n << 3;
        }
        if (size >= 2) {
            u32 n = (u32)size >> 1;
            u32 i;
            for (i = n; i != 0; i--) {
                *(s16*)d = *(const s16*)s;
                s += 2;
                d += 2;
            }
            size -= n << 1;
        }
        if (size >= 1) {
            *d = *s;
        }
    } else {
        // Words once both pointers are aligned.
        if (((uptr)(s) & 1) && size >= 1) {
            *d++ = *s++;
            size--;
        }
        if (((uptr)(s) & 2) && size >= 2) {
            *(s16*)d = *(const s16*)s;
            s += 2;
            size -= 2;
            d += 2;
        }
        if (size >= 16) {
            u32 n = (u32)size >> 4;
            u32 i;
            for (i = n; i != 0; i--) {
                u32 w1 = ((const u32*)s)[1];
                u32 w2 = ((const u32*)s)[2];
                u32 w3 = ((const u32*)s)[3];
                u32 w0 = ((const u32*)s)[0];
                s += 16;
                ((u32*)d)[0] = w0;
                ((u32*)d)[1] = w1;
                ((u32*)d)[2] = w2;
                ((u32*)d)[3] = w3;
                d += 16;
            }
            size -= n << 4;
        }
        if (size >= 4) {
            u32 n = (u32)size >> 2;
            u32 i;
            for (i = n; i != 0; i--) {
                *(u32*)d = *(const u32*)s;
                s += 4;
                d += 4;
            }
            size -= n << 2;
        }
        if (size >= 2) {
            *(s16*)d = *(const s16*)s;
            s += 2;
            size -= 2;
            d += 2;
        }
        if (size >= 1) {
            *d = *s;
        }
    }
}
