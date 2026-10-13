#include "ea/snd/sndi.h"

// Random value in -range..range, range in 1.16 (clamped to 0..1.0).
int randrange(int range) {
    if (range > 0x10000) {
        range = 0x10000;
    } else if (range < 0) {
        range = 0;
    }
    return (int)(((iSNDrandom() & 0x7FFF) - 0x4000) * range) >> 14;
}
