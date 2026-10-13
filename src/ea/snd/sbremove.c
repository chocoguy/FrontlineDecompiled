#include "ea/snd/sndi.h"

// Stop the bank's voices, tell user data clients its timbres are going, free its ARAM.
// -1 removes every bank.
extern "C" int SNDbankremove(int bank) {
    BANKVER5* data;
    int i;

    if (bank == -1) {
        for (i = 0; i < sndgs.opts.caps.maxBanks; i++) {
            SNDbankremove(i);
        }
        return 0;
    }
    if (SNDBANKI_valid(bank) != 0) {
        return -8;
    }
    SNDSYS_entercritical();
    data = sndgs.banks[bank].data;
    for (i = 0; i < sndgs.numVoices; i++) {
        SNDVOICE* v = &sndgs.voices[i];
        if (v->bank == bank) {
            SNDstop(v->handle);
        }
    }
    for (i = 0; i < data->numPatches; i++) {
        void* p = SNDBANKI_getppatch(data, i);
        if (p != NULL) {
            SNDIPATCHHEADER header;
            int more;
            p = (u8*)p + 4;
            do {
                more = SNDI_parsetimbre(&p, &header);
                SNDBANKI_userdatacallback(&header, 2);
            } while (more != 0);
        }
    }
    if (sndgs.banks[bank].aram != NULL) {
        SNDPLATFORM_memfree(8, (unsigned int)sndgs.banks[bank].aram);
    }
    sndgs.banks[bank].data = NULL;
    sndgs.banks[bank].busy = 0;
    SNDSYS_leavecritical();
    return 0;
}
