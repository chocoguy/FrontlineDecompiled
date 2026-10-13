#include "ea/snd/sndi.h"

extern "C" int SNDBANK_patchinfo(int bank, int patch, void* format, void* attr, u32* desc) {
    void* p;
    SNDSAMPLEFORMAT* fmt = (SNDSAMPLEFORMAT*)format;
    SNDSAMPLEATTR* a = (SNDSAMPLEATTR*)attr;
    int i;

    if (SNDBANKI_valid(bank) < 0) {
        return -8;
    }
    p = SNDBANKI_getppatch(sndgs.banks[bank].data, patch);
    if (p == NULL) {
        return -8;
    }
    SNDSYS_entercritical();
    SNDI_patchtohdr(sndgs.banks[bank].data, (TAGGEDPATCH*)p, fmt, a, (SNDSAMPLEDESC*)desc);
    for (i = 0; i < fmt->channels; i++) {
        if (a->data[i] != NULL) {
            SNDMEMI_free(a->data[i]);
            a->data[i] = NULL;
            a->size[i] = 0;
        }
    }
    SNDSYS_leavecritical();
    return 0;
}
