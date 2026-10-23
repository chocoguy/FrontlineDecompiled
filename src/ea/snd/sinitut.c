#include "ea/snd/smix.h"

// MicroTalk decoders (format 4).
extern "C" void MIXI_initunpackmt(void) {
    sndmix.decoderInit[9] = SFILTER_unpackmtfinit;
    sndmix.decoderSize[9] = GC_SIZE(0xD68, sizeof(UNPACKMTFSTATE));
    sndmix.decoderInit[11] = SFILTER_unpackmtpfinit;
    sndmix.decoderSize[11] = GC_SIZE(0xD74, sizeof(UNPACKMTPFSTATE));
}
