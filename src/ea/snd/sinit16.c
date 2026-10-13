#include "ea/snd/smix.h"

// 16-bit PCM decoders (formats 7, 8).
extern "C" void MIXI_initunpack16(void) {
    sndmix.decoderInit[3] = SFILTER_unpackfinit;
    sndmix.decoderSize[3] = 0x2C;
    sndmix.decoderInit[4] = SFILTER_unpacklfinit;
    sndmix.decoderSize[4] = 0x30;
    sndmix.decoderInit[5] = SFILTER_unpackpfinit;
    sndmix.decoderSize[5] = 0x34;
}
