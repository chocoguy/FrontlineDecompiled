#include "ea/snd/sndi.h"

// 0 for a loaded bank, -18 while it is being added, -8 otherwise.
int SNDBANKI_valid(int bank) {
    if (bank < 0 || bank >= sndgs.opts.caps.maxBanks) {
        return -8;
    }
    if (sndgs.banks[bank].data == NULL) {
        return -8;
    }
    return sndgs.banks[bank].busy ? -18 : 0;
}
