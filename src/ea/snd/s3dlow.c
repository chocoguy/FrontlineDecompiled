#include "ea/snd/sndi.h"

extern "C" int SND3dpos(int handle, int azimuth, int distance) {
    if (distance > 0x3FFF) {
        distance = 0x3FFF;
    } else if (distance < -0x4000) {
        distance = -0x4000;
    }
    int v = SNDVOICEI_get(handle);
    if (v >= 0) {
        int key = -1;
        while (iSNDpatchkey(v, &key)) {
            SNDVOICE* voice = &sndgs.voices[key];
            voice->azimuth = voice->azimuthBase + azimuth;
            voice->distance = distance;
            SNDPLATFORM_set3dpos(key);
        }
    }
    return v;
}
