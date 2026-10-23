#include "ea/snd/smix.h"

// 16-bit PCM decoders (formats 7, 8).
extern "C" void MIXI_initunpack16(void) {
    sndmix.decoderInit[3] = SFILTER_unpackfinit;
    sndmix.decoderSize[3] = GC_SIZE(0x2C, sizeof(UNPACKFSTATE));
    sndmix.decoderInit[4] = SFILTER_unpacklfinit;
    sndmix.decoderSize[4] = GC_SIZE(0x30, sizeof(UNPACKLFSTATE));
    sndmix.decoderInit[5] = SFILTER_unpackpfinit;
    sndmix.decoderSize[5] = GC_SIZE(0x34, sizeof(UNPACKPFSTATE));
}
