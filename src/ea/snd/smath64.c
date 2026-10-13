#include "ea/snd/sndi.h"

SINT64 iSNDmulu64(unsigned int a, unsigned int b) {
    SINT64 r;
    u32 al = a & 0xFFFF;
    u32 bl = b & 0xFFFF;
    u32 ah = a >> 16;
    u32 bh = b >> 16;
    u32 ll = al * bl;
    u32 lh = al * bh;
    u32 hl = ah * bl;
    u32 hh = ah * bh;
    u32 mid = (ll >> 16) + (lh & 0xFFFF) + (hl & 0xFFFF);

    r.lo = ll + ((lh << 16) + (hl << 16));
    r.hi = (lh >> 16) + ((hl >> 16) + hh + (mid >> 16));
    return r;
}

// value / divisor; the quotient must fit in 32 bits.
unsigned int iSNDdivu64(SINT64 value, int divisor) {
    u32 lo = value.lo;
    u32 rem = value.hi;
    u32 q = 0;
    int i;

    for (i = 0; i < 32; i++) {
        rem = (rem << 1) + (lo >> 31);
        q <<= 1;
        lo <<= 1;
        if (rem >= (u32)divisor) {
            rem -= divisor;
            q++;
        }
    }
    return q;
}
