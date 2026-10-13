#include "ea/snd/sndi.h"

extern "C" int SNDplaysetdef(SNDPLAYOPTS* opts) {
    opts->key = 60;
    opts->pitchmult = 0x1000;
    opts->timemult = 0x1000;
    opts->unk_0x10 = 0x1000;
    opts->unk_0x12 = 0;
    opts->lowpass = 0xFFFF;
    opts->highpass = 0;
    opts->unk_0x3 = 0x7F;
    opts->vol = 0x7F;
    opts->drylevel = 0x7F;
    opts->unk_0x1 = 0x40;
    opts->fxlevel = 0x7F;
    opts->azimuth = 0;
    opts->distance = 0;
    return 0;
}
