#include "ea/snd/smix.h"

// 16-bit PCM -> float decoder (unrolled x8 by the compiler).
void decode16x87(int frames, s16* src, f32* dest) {
    int i;

    for (i = 0; i < frames; i++) {
        dest[i] = (f32)SAMPLE_S16(&src[i]);
    }
}
