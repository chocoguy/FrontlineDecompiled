// The decoder's bit reader (MSSG getbits.c), reading the stream from
// memory: Bfr holds the next 32 bits, refilled a byte at a time.
#include "movie/mpeg2dec.h"

// Tops Bfr up from the stream until more than 24 bits are buffered.
static inline void Fill_Buffer(int Incnt) {
    while (Incnt <= 24) {
        MPDld->Bfr |= *MPDld->Rdptr++ << (24 - Incnt);
        Incnt += 8;
    }
    MPDld->Incnt = Incnt;
}

int Get_Bits32() {
    unsigned int l = MPDld->Bfr;
    MPDld->Bfr = 0;
    Fill_Buffer(MPDld->Incnt - 32);
    return l;
}

void Flush_Buffer32() {
    MPDld->Bfr = 0;
    Fill_Buffer(MPDld->Incnt - 32);
}

int Get_Bits(int n) {
    unsigned int bfr = MPDld->Bfr;
    MPDld->Bfr = bfr << n;
    unsigned int val = TOP_BITS(bfr, n);
    Fill_Buffer(MPDld->Incnt - n);
    return val;
}

void Flush_Buffer(int n) {
    MPDld->Bfr <<= n;
    Fill_Buffer(MPDld->Incnt - n);
}

int Get_Bits1() {
    unsigned int bfr = MPDld->Bfr;
    MPDld->Bfr = bfr << 1;
    unsigned int val = bfr >> 31;
    Fill_Buffer(MPDld->Incnt - 1);
    return val;
}

int Show_Bits(int n) {
    return TOP_BITS(MPDld->Bfr, n);
}

void Initialize_Buffer(unsigned char* buffer) {
    MPDld->Incnt = 0;
    MPDld->Rdptr = buffer;
    MPDld->Bfr = 0;
    Fill_Buffer(MPDld->Incnt);
}
