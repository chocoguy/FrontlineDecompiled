#include "ea/snd/sndi.h"

int SNDBANKI_alloc() {
    int i;
    for (i = 0; i < sndgs.opts.caps.maxBanks; i++) {
        if (sndgs.banks[i].data == NULL) {
            return i;
        }
    }
    return -9;
}

void* SNDBANKI_getppatch(BANKVER5* bank, int patch) {
    if (patch >= bank->numPatches) {
        return NULL;
    }
    if (bank->patchOffsets[patch] != 0) {
        return (u8*)bank + (patch * 4 + 0x14 + bank->patchOffsets[patch]);
    }
    return NULL;
}
