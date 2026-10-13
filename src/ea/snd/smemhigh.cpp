#include "ea/snd/sndi.h"

// Most of the pool ever used, in percent.
extern "C" int SNDMEM_gethighwater(void) {
    SNDMEMPOOL* pool = sndgs.mem;
    return (pool->size - pool->lowwater) * 100 / pool->size;
}
