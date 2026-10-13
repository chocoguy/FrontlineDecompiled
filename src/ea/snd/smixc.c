#include "ea/snd/smix.h"

extern "C" void mixc(int frames, f32* src, f32* dest, f32 gain) {
    for (int i = 0; i < frames; i++) {
        *dest = *dest + gain * *src;
        src++;
        dest++;
    }
}
