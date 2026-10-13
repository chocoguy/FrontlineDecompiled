#include "ea/sys/mem.h"

// Fill with stores as wide as the alignment allows. Words take `value` whole, so a byte
// pattern must already be repeated in it.
extern "C" void MEM_fill(void* dest, int value, int size) {
    u8* d = (u8*)dest;

    if (size >= 1 && ((u32)d & 1)) {
        *d = value;
        d += 1;
        size -= 1;
    }
    if (size >= 2 && ((u32)d & 2)) {
        *(u16*)d = value;
        d += 2;
        size -= 2;
    }
    if (size >= 4 && ((u32)d & 4)) {
        *(u32*)d = value;
        d += 4;
        size -= 4;
    }
    if (size >= 8 && ((u32)d & 8)) {
        ((u32*)d)[0] = value;
        size -= 8;
        ((u32*)d)[1] = value;
        d += 8;
    }
    if (size >= 16 && ((u32)d & 16)) {
        ((u32*)d)[0] = value;
        size -= 16;
        ((u32*)d)[1] = value;
        ((u32*)d)[2] = value;
        ((u32*)d)[3] = value;
        d += 16;
    }
    if (size >= 32) {
        u32 n = (u32)size >> 5;
        u32 i;
        for (i = n; i != 0; i--) {
            ((u32*)d)[0] = value;
            ((u32*)d)[1] = value;
            ((u32*)d)[2] = value;
            ((u32*)d)[3] = value;
            ((u32*)d)[4] = value;
            ((u32*)d)[5] = value;
            ((u32*)d)[6] = value;
            ((u32*)d)[7] = value;
            d += 32;
        }
        size -= n << 5;
    }
    if (size >= 16) {
        ((u32*)d)[0] = value;
        size -= 16;
        ((u32*)d)[1] = value;
        ((u32*)d)[2] = value;
        ((u32*)d)[3] = value;
        d += 16;
    }
    if (size >= 8) {
        ((u32*)d)[0] = value;
        size -= 8;
        ((u32*)d)[1] = value;
        d += 8;
    }
    if (size >= 4) {
        *(u32*)d = value;
        d += 4;
        size -= 4;
    }
    if (size >= 2) {
        *(u16*)d = value;
        d += 2;
        size -= 2;
    }
    if (size >= 1) {
        *d = value;
    }
}
