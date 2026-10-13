#include "ea/snd/smix.h"

// MicroTalk decoders (format 4).
extern "C" void MIXI_initunpackmt(void) {
    sndmix.decoderInit[9] = SFILTER_unpackmtfinit;
    sndmix.decoderSize[9] = 0xD68;
    sndmix.decoderInit[11] = SFILTER_unpackmtpfinit;
    sndmix.decoderSize[11] = 0xD74;
}
