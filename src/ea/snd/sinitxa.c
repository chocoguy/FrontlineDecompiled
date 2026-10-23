#include "ea/snd/smix.h"

// XA ADPCM decoders (format 10).
extern "C" void MIXI_initunpackxa(void) {
    sndmix.decoderInit[6] = SFILTER_unpackxafinit;
    sndmix.decoderSize[6] = GC_SIZE(0x2C, sizeof(UNPACKXAFSTATE));
    sndmix.decoderSize[7] = GC_SIZE(0x44, sizeof(UNPACKXALFSTATE));
    sndmix.decoderInit[7] = SFILTER_unpackxalfinit;
    sndmix.decoderInit[8] = SFILTER_unpackxapfinit;
    sndmix.decoderSize[8] = GC_SIZE(0x38, sizeof(UNPACKXAPFSTATE));
}
