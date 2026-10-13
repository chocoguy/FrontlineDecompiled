#include "ea/snd/smix.h"

// XA ADPCM decoders (format 10).
extern "C" void MIXI_initunpackxa(void) {
    sndmix.decoderInit[6] = SFILTER_unpackxafinit;
    sndmix.decoderSize[6] = 0x2C;
    sndmix.decoderSize[7] = 0x44;
    sndmix.decoderInit[7] = SFILTER_unpackxalfinit;
    sndmix.decoderInit[8] = SFILTER_unpackxapfinit;
    sndmix.decoderSize[8] = 0x38;
}
