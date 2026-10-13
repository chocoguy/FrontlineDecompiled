#include "ea/snd/sndi.h"

// Nonzero once the sound has finished.
extern "C" int SNDover(int handle) {
    return (unsigned int)SNDVOICEI_get(handle) >> 31;
}
