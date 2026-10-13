// EA MicroTalk decoder (sdecmut) and its mixer filters (supmutf, supmutpf).
#ifndef _EA_SND_SDECMUT_H_
#define _EA_SND_SDECMUT_H_

#include "types.h"

// size 0xD40
struct UTALKSTATE {
    /* 0x000 */ u8* ptr;
    /* 0x004 */ u32 bits;
    /* 0x008 */ s32 nbits;
    /* 0x00C */ s32 multipulseThresh;
    /* 0x010 */ f32 gains[64];
    /* 0x110 */ f32 rc[12];
    /* 0x140 */ f32 history[12];
    /* 0x170 */ f32 cb[324];  // previous output (adaptive codebook)
    /* 0x680 */ f32 frame[432];
};

void initmut(char* data, UTALKSTATE* ut);
void decodemut(UTALKSTATE* ut);
void readsamples(UTALKSTATE* ut, int multipulse, f32* out);

#endif
