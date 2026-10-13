// EA XA ADPCM block decoder to float (eaxadecf.cpp), allocated through the CODA hooks (coda.cpp).
#ifndef _EA_SND_EAXADECF_H_
#define _EA_SND_EAXADECF_H_

#include "types.h"

// The sound library keeps functions out of line unless they are declared inline.
#pragma auto_inline off

namespace SND {

extern void* (*CODANew)(unsigned long size);
extern void (*CODADelete)(void* ptr);
void CODASetNew(void* (*fn)(unsigned long));
void CODASetDelete(void (*fn)(void*));

// Predictor history carried across blocks.
struct XAFSTATE {
    f32 s1;  // last sample
    f32 s2;  // the one before
};

// A run of 15-byte blocks (a header byte: filter << 4 | shift, then 28 nibbles) to decode.
struct MXAPACKETF {
    /* 0x00 */ s32 count;  // samples, consumed in steps of 28
    /* 0x04 */ f32 s1;
    /* 0x08 */ f32 s2;
    /* 0x0C */ u8* src;   // restored on return
    /* 0x10 */ f32* dst;  // dst[-2], dst[-1] hold the history while decoding
};

void decodexac(MXAPACKETF* p);

extern f32 xafilterf[2][4];
extern f32 xatablef[16][16];

class CEAXABLKDecf {
public:
    void* operator new(unsigned long size);
    void operator delete(void* ptr);
    CEAXABLKDecf();
    int Feed(void* data, int size, int samples);
    int Decode(f32** out, int samples);
    XAFSTATE GetState();
    void SetState(XAFSTATE* state);

    /* 0x00 */ u8 unk_0x0[0x4];
    /* 0x04 */ s32 buffered;   // samples of a partial block waiting in buf
    /* 0x08 */ s32 remaining;  // samples left to output
    /* 0x0C */ s32 size;
    /* 0x10 */ f32* bufPos;
    /* 0x14 */ u8 unk_0x14[0x8];
    /* 0x1C */ f32 history[2];
    /* 0x24 */ f32 buf[28];
    /* 0x94 */ MXAPACKETF pkt;
};

}  // namespace SND

#endif
