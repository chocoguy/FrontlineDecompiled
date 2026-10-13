#include "ea/snd/sndi.h"

extern "C" int SNDBANK_play(int bank, int patch, SNDPLAYOPTS* opts) {
    BANKVER5* data;
    if (SNDBANKI_valid(bank) < 0) {
        return -8;
    }
    data = sndgs.banks[bank].data;
    if (patch < 0 || patch >= data->numPatches) {
        return -8;
    }
    return SNDBANKI_playpatch(data, (TAGGEDPATCH*)SNDBANKI_getppatch(data, patch), bank, patch, opts);
}
