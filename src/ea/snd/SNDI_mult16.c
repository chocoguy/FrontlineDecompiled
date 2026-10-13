#include "ea/snd/sndi.h"

// Prime number of 16-sample blocks for ms * rate / 1000 samples, in samples.
int SNDI_findmult16(int ms, int rate) {
    return SNDI_findprime(ms, rate / 16) * 16;
}
